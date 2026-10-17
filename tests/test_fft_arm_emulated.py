#!/usr/bin/env python3
"""Run the actual ARM FFT execution and scalar oracle under Unicorn, without an ARM sysroot.

Development dependencies: clang with LLD, unicorn==2.1.4, pyelftools==0.32.
Example: python tests/test_fft_arm_emulated.py --clang /path/to/clang --arch arm64
This validates integer results and memory boundaries. Emulation provides no performance evidence.
The generated freestanding shim supplies memcpy/integer division/error return and immutable tables;
the production FFT execution, input validation and independent scalar arithmetic are unchanged.
"""
import argparse
import hashlib
import itertools
import math
from pathlib import Path
import random
import struct
import subprocess

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_ARCH_ARM64, UC_MODE_ARM, UC_MODE_LITTLE_ENDIAN
from unicorn import arm_const, arm64_const


ROOT = Path(__file__).resolve().parents[1]
DATA = 0x300000
PLAN, REVERSE, TWIDDLE = DATA, DATA + 0x1000, DATA + 0x20000
INPUT, OUTPUT, REFERENCE = DATA + 0x50000, DATA + 0x70000, DATA + 0x90000
STACK, RETURN = DATA + 0x200000 - 16, 0x700000
CANARY = b"\x5a" * 32


def run(command):
    subprocess.run([str(arg) for arg in command], check=True)


def build(args, directory):
    directory.mkdir(parents=True, exist_ok=True)
    # Clang's freestanding resource headers provide stdint/stddef/arm_neon, but no libc.
    for name, declarations in {
        "math.h": "double sin(double); double cos(double); long lround(double);\n",
        "stdlib.h": "#include <stddef.h>\nvoid *malloc(size_t); void *calloc(size_t,size_t); void free(void *);\n",
        "string.h": "#include <stddef.h>\nvoid *memcpy(void *,const void *,size_t);\n",
    }.items():
        (directory / name).write_text(declarations, encoding="utf-8")
    fft = args.source_root / "src/dsp/fft/fft.c"
    kernel = args.source_root / "src/dsp/arm/fft_neon_intrinsics.c"
    simd = kernel.exists()
    if not simd:
        if args.arch != "arm64":
            raise RuntimeError("Older ARM32 source has no NEON kernel; use the current source tree")
        kernel = args.source_root / "src/dsp/arm/fft_neon.S"
    print(f"# arch={args.arch},fft_sha256={hashlib.sha256(fft.read_bytes()).hexdigest()},"
          f"kernel={kernel.name},kernel_sha256={hashlib.sha256(kernel.read_bytes()).hexdigest()}",
          flush=True)
    shim = directory / "shim.c"
    shim.write_text(
        '#include "' + fft.as_posix() + '"\n'
        'void *memcpy(void *to,const void *from,size_t count) {\n'
        ' unsigned char *d=to;const unsigned char *s=from;size_t i;\n'
        ' for(i=0;i<count;++i)d[i]=s[i];return to; }\n'
        'int NgFail(TNgError *error,const char *format,...) {\n'
        ' (void)error;(void)format;return NG_ERROR; }\n'
        'unsigned __aeabi_uidiv(unsigned numerator,unsigned denominator) {\n'
        ' unsigned result=0,bit=1;if(denominator==0)return 0;\n'
        ' while(denominator<=(numerator>>1)){denominator<<=1;bit<<=1;}\n'
        ' while(bit!=0){if(numerator>=denominator){numerator-=denominator;result|=bit;}\n'
        ' denominator>>=1;bit>>=1;}return result; }\n'
        'void NgCrossPlanInit(TNgFftPlan *p,size_t maximum,uint32_t *reverse,int16_t *twiddles) {\n'
        ' size_t n;p->maximum=maximum;p->bits=0;\n'
        ' for(n=maximum;n>1;n>>=1)++p->bits;p->reverse=reverse;p->twiddles=twiddles; }\n',
        encoding="utf-8",
    )
    target = ["--target=aarch64-none-elf"] if args.arch == "arm64" else [
        "--target=armv7a-none-eabihf", "-mfpu=neon", "-mfloat-abi=hard", "-marm"
    ]
    common = [args.clang, *target, "-O3", "-Wall", "-Wextra", "-Werror", "-ffreestanding",
              "-ffunction-sections", "-fdata-sections", "-I", directory,
              "-I", ROOT / "include", "-I", ROOT / "src"]
    definitions = ["-DNGENE_FFT_SIMD=1"] if simd else ["-DNGENE_FFT_ASSEMBLY=1"]
    run([*common, *definitions, '-DNGENE_FFT_BACKEND="NEON-emulated"', "-c", shim,
         "-o", directory / "fft.o"])
    run([*common, "-c", kernel, "-o", directory / "kernel.o"])
    elf = directory / "fft.elf"
    run([args.clang, *target, "-nostdlib", "-fuse-ld=lld", "-Wl,--gc-sections",
         "-Wl,--image-base=0x10000", "-Wl,-Ttext=0x100000", "-Wl,--entry=NgFftExecute", "-Wl,-u,NgFftReference",
         "-Wl,-u,NgCrossPlanInit", directory / "fft.o", directory / "kernel.o", "-o", elf])
    return elf


class Machine:
    def __init__(self, elf_path, arch):
        self.arch = arch
        self.uc = Uc(UC_ARCH_ARM64 if arch == "arm64" else UC_ARCH_ARM,
                     UC_MODE_LITTLE_ENDIAN if arch == "arm64" else UC_MODE_ARM)
        with elf_path.open("rb") as stream:
            elf = ELFFile(stream)
            segments = [p for p in elf.iter_segments() if p["p_type"] == "PT_LOAD"]
            first = min(p["p_vaddr"] for p in segments) & ~4095
            last = (max(p["p_vaddr"] + p["p_memsz"] for p in segments) + 4095) & ~4095
            self.uc.mem_map(first, last - first)
            for segment in segments:
                self.uc.mem_write(segment["p_vaddr"], segment.data())
            self.symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
        self.uc.mem_map(DATA, 0x200000)
        self.uc.mem_map(RETURN, 4096)
        if arch == "arm64":
            self.args = [getattr(arm64_const, f"UC_ARM64_REG_X{i}") for i in range(8)]
            self.sp, self.lr = arm64_const.UC_ARM64_REG_SP, arm64_const.UC_ARM64_REG_LR
            self.uc.reg_write(arm64_const.UC_ARM64_REG_CPACR_EL1, 3 << 20)
        else:
            self.args = [getattr(arm_const, f"UC_ARM_REG_R{i}") for i in range(4)]
            self.sp, self.lr = arm_const.UC_ARM_REG_SP, arm_const.UC_ARM_REG_LR
            self.uc.reg_write(arm_const.UC_ARM_REG_C1_C0_2, 15 << 20)
            self.uc.reg_write(arm_const.UC_ARM_REG_FPEXC, 1 << 30)
        self.uc.reg_write(self.sp, STACK)

    def call(self, name, *arguments):
        self.uc.reg_write(self.sp, STACK)
        self.uc.reg_write(self.lr, RETURN)
        for i, value in enumerate(arguments):
            if i < len(self.args):
                self.uc.reg_write(self.args[i], value)
            else:
                self.uc.mem_write(STACK + 4 * (i - len(self.args)), struct.pack("<I", value))
        self.uc.emu_start(self.symbols[name], RETURN, count=50_000_000)
        pc = arm64_const.UC_ARM64_REG_PC if self.arch == "arm64" else arm_const.UC_ARM_REG_PC
        assert self.uc.reg_read(pc) == RETURN, f"{name} exceeded its instruction limit"
        return self.uc.reg_read(self.args[0]) & 0xffffffff

    def plan(self, maximum):
        bits = maximum.bit_length() - 1
        reverse = [int(f"{i:0{bits}b}"[::-1], 2) for i in range(maximum)]
        coefficients = []
        width = 2
        while width <= maximum:
            for i in range(width // 2):
                phase = 2 * math.pi * i / width
                # C lround: halfway cases round away from zero.
                rounded = lambda x: math.floor(x + 0.5) if x >= 0 else math.ceil(x - 0.5)
                cosine, sine = rounded(16384 * math.cos(phase)), rounded(-16384 * math.sin(phase))
                coefficients.extend((cosine, -sine, sine, cosine))
            width *= 2
        self.uc.mem_write(REVERSE, struct.pack(f"<{len(reverse)}I", *reverse))
        self.uc.mem_write(TWIDDLE, struct.pack(f"<{len(coefficients)}h", *coefficients))
        self.call("NgCrossPlanInit", PLAN, maximum, REVERSE, TWIDDLE)

    def compare(self, data, input_lane=1, output_lane=1):
        count = len(data) // 2
        payload = struct.pack(f"<{len(data)}h", *data)
        source, destination = INPUT + 32 + 2 * input_lane, OUTPUT + 32 + 2 * output_lane
        self.uc.mem_write(source - 32, CANARY + payload + CANARY)
        self.uc.mem_write(destination - 32, CANARY + b"\x5a" * len(payload) + CANARY)
        assert self.call("NgFftExecute", PLAN, count, source, destination, 0) == 0
        assert self.call("NgFftReference", PLAN, count, source, REFERENCE, 0) == 0
        actual = bytes(self.uc.mem_read(destination, len(payload)))
        expected = bytes(self.uc.mem_read(REFERENCE, len(payload)))
        if actual != expected:
            got, want = struct.unpack(f"<{len(data)}h", actual), struct.unpack(f"<{len(data)}h", expected)
            index = next(i for i in range(len(data)) if got[i] != want[i])
            raise AssertionError(f"N={count},component={index},SIMD={got[index]},oracle={want[index]}")
        assert bytes(self.uc.mem_read(source - 32, len(payload) + 64)) == CANARY + payload + CANARY
        assert bytes(self.uc.mem_read(destination - 32, 32)) == CANARY
        assert bytes(self.uc.mem_read(destination + len(payload), 32)) == CANARY
        return struct.unpack(f"<{len(data)}h", actual)


def validate(machine, exhaustive):
    rng = random.Random(0x4e454f4e)
    cases = 0
    sizes = [1 << bits for bits in range(3, 15)]
    machine.plan(16384)
    for n in sizes:
        patterns = [[0] * (2 * n), [8192] + [0] * (2 * n - 1),
                    [rng.randint(-16384, 16384) for _ in range(2 * n)],
                    [rng.choice((-16384, 16384)) for _ in range(2 * n)],
                    [rng.choice((-16383, -1, 1, 16383)) for _ in range(2 * n)]]
        for data in patterns:
            machine.compare(data)
            cases += 1
        tone = [round(8000 * f(2 * math.pi * 3 * i / n)) for i in range(n) for f in (math.cos, math.sin)]
        out = machine.compare(tone)
        assert abs(out[6] - 8000) < 20 and abs(out[7]) < 20
        assert all(abs(value) < 20 for i, value in enumerate(out) if i != 6)
        cases += 1
        # Independent input/output offsets exercise every alignment accepted by the public ABI.
        for input_lane, output_lane in itertools.product(range(8), repeat=2):
            machine.compare(patterns[2], input_lane, output_lane)
            cases += 1
        source = INPUT + 32
        payload = bytearray(4 * n)
        for lane in range(min(32, 2 * n)):
            for position in (lane, 2 * n - 1 - lane):
                for value in (-32768, -16385, 16385, 32767):
                    struct.pack_into("<h", payload, 2 * position, value)
                    machine.uc.mem_write(source, bytes(payload))
                    assert machine.call("NgFftExecute", PLAN, n, source, OUTPUT, 0) == 0xffffffff
                    cases += 1
                struct.pack_into("<h", payload, 2 * position, 0)
        print(f"  {machine.arch}: N={n} arithmetic/alignment/rejection passed", flush=True)
    for maximum in sizes:
        machine.plan(maximum)
        for n in sizes:
            if n > maximum:
                break
            for _ in range(3):
                machine.compare([rng.randint(-16384, 16384) for _ in range(2 * n)])
                cases += 1
    if exhaustive:
        machine.plan(8)
        pairs = ((-16384, -16384), (16384, -16383), (1, -1))
        for rotation in range(2):
            for selection in itertools.product(pairs, repeat=8):
                data = []
                for i, (re, im) in enumerate(selection):
                    for _ in range(rotation * (i & 3)):
                        re, im = -im, re
                    data.extend((re, im))
                machine.compare(data)
                cases += 1
    print(f"PASS: {machine.arch} actual NEON instructions, {cases} guarded bit-exact/rejection cases; "
          "all lengths, plan/subsizes, 64 alignments, analytic tones. No performance claim.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--arch", choices=("arm", "arm64"), required=True)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/fft-arm-emulated")
    parser.add_argument("--skip-exhaustive", action="store_true")
    args = parser.parse_args()
    args.source_root = args.source_root.resolve()
    directory = args.build_dir.resolve() / args.arch
    elf = build(args, directory)
    validate(Machine(elf, args.arch), not args.skip_exhaustive)


if __name__ == "__main__":
    main()
