# Maintaining the GitHub Wiki

The [NGene operator manual](https://github.com/mauriziofantino/nav-ngene-evo/wiki) is
maintained in GitHub's separate Wiki repository. That repository is the source of truth
for its pages, sidebar, footer and screenshots; do not keep a second copy in this repository.

## Edit and publish

From the receiver repository root, clone the Wiki once:

```sh
git clone https://github.com/mauriziofantino/nav-ngene-evo.wiki.git build/github-wiki
```

Before editing an existing checkout, confirm its remote and pull the current pages:

```sh
git -C build/github-wiki remote -v
git -C build/github-wiki pull --ff-only
```

Edit the Markdown pages and `images/` directly in that checkout. Maintain English text,
page links, `_Sidebar.md` and `_Footer.md` as appropriate. Review and publish the changes:

```sh
git -C build/github-wiki status --short
git -C build/github-wiki diff
git -C build/github-wiki add --all
git -C build/github-wiki commit -m "Update NGene operator manual"
git -C build/github-wiki push origin HEAD
```

Use the Wiki's existing default branch. No architecture-specific or publication branches
are needed. GitHub documents this [local Wiki workflow](https://docs.github.com/en/communities/documenting-your-project-with-wikis/adding-or-editing-wiki-pages#adding-or-editing-wiki-pages-locally).

## Refresh GUI screenshots

Build NGene and provide Node.js, Playwright and a Chromium-compatible browser. For example,
on Windows:

```powershell
$env:NGENE_EXECUTABLE = 'build/msvc-x64-multisdr/Release/ngene.exe'
$env:BROWSER_EXECUTABLE = 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'
node scripts/capture-wiki.cjs --output build/github-wiki/images
```

If Playwright is outside normal module resolution, set `PLAYWRIGHT_MODULE` to its module
directory. The helper starts its own receiver with a private profile and deterministic
CI8 replay fixture, captures the real GUI, and stops that instance. It requires no SDR
hardware and does not change another receiver's profile or session.

Review the images before committing them in the Wiki repository. Keep their captions
clear that this is a replay demonstration, not an over-the-air GNSS capture or a position
solution. If the README illustration also needs updating, copy `receiver-dashboard.png`
to `docs/images/` and commit that image separately in the main receiver repository.
