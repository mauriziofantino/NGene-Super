"""Execute production AArch64 search/tracking kernels under Unicorn; no performance claim.

Uses the development dependencies and ELF loader from test_fft_arm_emulated.py.
The only host hooks are libc math, never the SIMD kernels being checked.
"""
import argparse
import math
from pathlib import Path
import random
import struct
import subprocess
from test_fft_arm_emulated import Machine, ROOT, DATA
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_D0, UC_ARM64_REG_D1, UC_ARM64_REG_X0, UC_ARM64_REG_X30, UC_ARM64_REG_PC

parser = argparse.ArgumentParser()
parser.add_argument('--clang', required=True)
args = parser.parse_args()
directory = ROOT / 'build/search-arm-emulated'
directory.mkdir(parents=True, exist_ok=True)
(directory/'math.h').write_text('double cos(double);double sin(double);double fmod(double,double);long long llround(double);long lround(double);double ldexp(double,int);\n#define fabs __builtin_fabs\n#define fmax __builtin_fmax\n#define sqrt __builtin_sqrt\n#define floor __builtin_floor\n#define isfinite __builtin_isfinite\n')
(directory/'stdlib.h').write_text('#include <stddef.h>\nvoid *calloc(size_t,size_t);void free(void*);\n')
(directory/'string.h').write_text('#include <stddef.h>\nvoid *memcpy(void*,const void*,size_t);void *memset(void*,int,size_t);\n')
shim = directory/'shim.c'
shim.write_text('''
#include "''' + (ROOT/'src/dsp/search.c').as_posix() + '''"
#include "''' + (ROOT/'src/dsp/tracking_replica.c').as_posix() + '''"
#include "''' + (ROOT/'src/dsp/gps_ca.c').as_posix() + '''"
int NgFail(TNgError *e,const char *s,...){(void)e;(void)s;return NG_ERROR;}
__attribute__((noinline)) double cos(double x){return x+1;}
__attribute__((noinline)) double sin(double x){return x+2;}
__attribute__((noinline)) double fmod(double x,double y){return x+y;}
__attribute__((noinline)) long long llround(double x){return (long long)x;}
#undef sqrt
__attribute__((noinline)) double sqrt(double x){return x+3;}
int Int32(size_t n,size_t *reverse,int32_t *twiddles,const int32_t *input,int32_t *output,int inverse)
{
    TNgSearchFft plan;
    plan.size=n;
    plan.reverse=reverse;
    plan.integerTwiddles=twiddles;
    return NgSearchFftExecuteInt32(&plan,input,output,inverse,0);
}
void Carrier(TNgComplex *out,size_t n,const double *p,int square){NgSearchCarrier(out,n,p[0],p[1],p[2],square);}
void Code(float *out,size_t n,const int8_t *chips,const double *p){NgSearchCode(out,n,chips,p[0],p[1],p[2]);}
void Phases(double *out,size_t n,const double *p){NgTrackingCodePhases(out,n,p[0],p[1],p[2]);}
void Replica(float *out,const double *p,size_t n,const int8_t *chips,unsigned period,const double *offset,int boc){NgTrackingReplica(out,p,n,chips,period,*offset,boc);}
''')
elf=directory/'search.elf'
exports=['Int32','Carrier','Code','Phases','Replica','NgSearchMix','NgSearchCorrelate','NgSearchFftExecute','cos','sin','fmod','llround','sqrt']
subprocess.run([args.clang,'--target=aarch64-none-elf','-O3','-ffp-contract=off','-Wall','-Wextra','-Werror',
    '-DNGENE_EXPERIMENTAL_INT32_NEON=1','-ffreestanding','-ffunction-sections','-fdata-sections','-I',str(directory),'-I',str(ROOT/'include'),'-I',str(ROOT/'src'),
    '-nostdlib','-fuse-ld=lld','-Wl,--gc-sections','-Wl,--image-base=0x10000','-Wl,-Ttext=0x100000','-Wl,--entry=Carrier',
    *['-Wl,-u,'+name for name in exports],str(shim),'-o',str(elf)],check=True)
m=Machine(elf,'arm64')
hooks={m.symbols[name]:name for name in ['cos','sin','fmod','llround','sqrt']}
def libc(uc,address,size,data):
    name=hooks.get(address)
    if name is None:return
    x=struct.unpack('<d',struct.pack('<Q',uc.reg_read(UC_ARM64_REG_D0)))[0]
    if name=='llround':uc.reg_write(UC_ARM64_REG_X0,math.floor(x+0.5) if x>=0 else math.ceil(x-0.5))
    else:
        y=struct.unpack('<d',struct.pack('<Q',uc.reg_read(UC_ARM64_REG_D1)))[0]
        value=math.fmod(x,y) if name=='fmod' else getattr(math,name)(x)
        uc.reg_write(UC_ARM64_REG_D0,struct.unpack('<Q',struct.pack('<d',value))[0])
    uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_X30))
libc_hook = m.uc.hook_add(UC_HOOK_CODE,libc)
A,B,C,P,OUT=DATA+0x1000,DATA+0x20000,DATA+0x40000,DATA+0x60000,DATA+0x80000
def put(address,fmt,values):m.uc.mem_write(address,struct.pack('<'+fmt*len(values),*values))
def get(address,fmt,n):return struct.unpack('<'+fmt*n,m.uc.mem_read(address,struct.calcsize(fmt)*n))
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
random.seed(732)
chips=[1 if random.getrandbits(1) else -1 for _ in range(8184)]
put(C,'b',chips)
for n in [1,2,3,37,2049]:
    for square in [0,1]:
        put(P,'d',[16367600,12345.67,0.17]);m.call('Carrier',OUT,n,P,square)
        actual=get(OUT,'f',2*n)
        for k in range(n):
            phase=-2*math.pi*(0.17+k*12345.67/16367600)
            expected=[math.cos(phase),math.sin(phase)]
            for j in range(2):assert abs(actual[2*k+j]-((-1 if expected[j]<0 else 1) if square else expected[j]))<1e-6,(n,square,k,j,actual[2*k+j],expected[j])
    values=[f32(random.uniform(-127,127)) for _ in range(2*n)]
    carrier=[f32(random.uniform(-1,1)) for _ in range(2*n)]
    put(A,'f',values);put(B,'f',carrier);m.call('NgSearchMix',A,B,n)
    mixed=get(A,'f',2*n)
    for k in range(n):
        assert mixed[2*k]==f32(f32(values[2*k]*carrier[2*k])-f32(values[2*k+1]*carrier[2*k+1]))
        assert mixed[2*k+1]==f32(f32(values[2*k]*carrier[2*k+1])+f32(values[2*k+1]*carrier[2*k]))
    signs=[float(chips[k]) for k in range(n)];put(B,'f',signs)
    for xor in [0,1]:
        m.call('NgSearchCorrelate',A,B,n,xor,OUT,OUT+8)
        actual=get(OUT,'d',2)
        for j in range(2):assert abs(actual[j]-sum(mixed[2*k+j]*signs[k] for k in range(n)))<1e-8
    put(P,'d',[0.123456,1023000.123,16367600]);m.call('Phases',A,n,P)
    phases=get(A,'d',n)
    for k in range(n):assert phases[k]==0.123456+k*1023000.123/16367600
    for boc,period in [(0,1023),(1,4092)]:
        for offset in [-0.4,0,0.4]:
            put(P,'d',[offset]);m.call('Replica',OUT,A,n,C,period,P,boc)
            for k,value in enumerate(get(OUT,'f',n)):
                phase=math.fmod(math.fmod(phases[k]-offset,period)+period,period)
                assert value==chips[int(phase*(2 if boc else 1))]
    for rate in [1023000.0,1023000.123]:
        put(P,'d',[16367600,rate,1022.87]);m.call('Code',OUT,n,C,P)
        for k,value in enumerate(get(OUT,'f',n)):
            # Q32 rounding for fractional rates; rational arithmetic for integers.
            if rate==int(rate):index=((round(1022.87*16367600)+k*int(rate))//16367600)%1023
            else:index=((int(1022.87*2**32)+k*int(rate/16367600*2**32+0.5))>>32)%1023
            assert value==chips[index]
for n in [8,16,64]:
    values=[f32(random.uniform(-1,1)) for _ in range(2*n)];put(A,'f',values)
    bits=n.bit_length()-1;put(B,'Q',[int(f'{k:0{bits}b}'[::-1],2) for k in range(n)])
    # Radix-2^2 pair tables, as NgSearchFftCreate: duplicated W1 then W2 parts per pair.
    table=[];half=8 if bits%2 else 4
    while half<n:
        for width in (2*half,4*half):
            table+=[f32(math.cos(2*math.pi*k/width)) for k in range(half) for _ in (0,1)]
            table+=[f32(-math.sin(2*math.pi*k/width)) for k in range(half) for _ in (0,1)]
        half*=4
    put(C,'f',table or [0.0])
    put(P,'Q',[n,B,C]);m.call('NgSearchFftExecute',P,A,0)
    actual=get(A,'f',2*n)
    for k in range(n):
        expected=sum(complex(values[2*j],values[2*j+1])*complex(math.cos(-2*math.pi*k*j/n),math.sin(-2*math.pi*k*j/n)) for j in range(n))
        assert abs(complex(actual[2*k],actual[2*k+1])-expected)<2e-5
    m.call('NgSearchFftExecute',P,A,1)
    assert max(abs(a-b) for a,b in zip(get(A,'f',2*n),values))<1e-6
print('AArch64 NEON carrier, product/XOR, phase rotation, code/replica and FFT execution passed')

def truncate(value, divisor):
    return -(abs(value) // divisor) if value < 0 else value // divisor


def scalar_fft(values, reverse, stages, inverse):
    n = len(reverse)
    result = [component for k in reverse for component in values[2*k:2*k+2]]
    for coefficients in stages:
        half = len(coefficients)
        for base in range(0, n, 2 * half):
            for k, (wr, wi) in enumerate(coefficients):
                a = 2 * (base + k)
                b = a + 2 * half
                if inverse:
                    wi = -wi
                br, bi = result[b:b+2]
                r = truncate(br * wr - bi * wi, 1 << 30)
                q = truncate(br * wi + bi * wr, 1 << 30)
                ar, ai = result[a:a+2]
                result[a:a+2] = ar + r, ai + q
                result[b:b+2] = ar - r, ai - q
    return tuple(truncate(x, n) for x in result) if inverse else tuple(result)


m.uc.hook_del(libc_hook)
rng = random.Random(20261121)
components = 0
for n in (1024, 2048, 4096, 8192, 16384):
    bits = n.bit_length() - 1
    reverse = [int(f'{k:0{bits}b}'[::-1], 2) for k in range(n)]
    stages = []
    for shift in range(bits):
        half = 1 << shift
        stages.append([(round(math.cos(-math.pi*k/half) * (1 << 30)),
                        round(math.sin(-math.pi*k/half) * (1 << 30))) for k in range(half)])
    put(B, 'Q', reverse)
    put(C, 'i', [part for stage in stages for twiddle in stage for part in twiddle])
    input_address = DATA + 0xa0000
    fixtures = [
        [0] * (2*n),
        [16384 if k == 2*(n//3) else 0 for k in range(2*n)],
        [-16384] * (2*n),
        [-16384 if k & 2 else 16384 for k in range(2*n)],
        [k % 17 - 8 for k in range(2*n)],
        [rng.randint(-16384, 16384) for _ in range(2*n)],
    ]
    for index, values in enumerate(fixtures):
        put(input_address, 'i', values)
        for inverse in (0, 1):
            assert m.call('Int32', n, B, C, input_address, OUT, inverse) == 0
            actual = get(OUT, 'i', 2*n)
            expected = scalar_fft(values, reverse, stages, inverse)
            assert actual == expected, (n, index, inverse)
            components += 2*n
    print(f'AArch64 int32 FFT N={n}: 12 transforms, bit-exact')
print(f'AArch64 int32 FFT: {components} I/Q components match the scalar reference')
