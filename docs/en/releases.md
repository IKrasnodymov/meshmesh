# Versions, builds and publishing

`versions.properties` is the single source of public versions: `firmware` for all boards and
`android` for the app. They are independent `MAJOR.MINOR.PATCH` values: incompatible behaviour,
compatible features, then fixes. Current releases are firmware 0.6.0 and Android 0.6.0. This does not change
the MeshCore wire protocol version.

Before a release, bump the affected component and run:

```sh
python tools/version.py --generate
python tools/version.py --check
python tools/version_check.py
```

The generated `include/Version.h` is committed and checked by PlatformIO. Firmware `status`
reports `version`, Git `revision` (with `-dirty` for local changes) and the image's `build_sha256`.
A matching version never replaces a hash check. Package `manifest.json` includes the version and
revision; site generation rejects packages from different revisions.

Android displays `0.6.0+<versionCode>`. Published codes are
`1000000 + github.run_number * 100 + github.run_attempt`, passed as `MM_VERSION_CODE`.
Each retry increases the code, and the next run is higher (attempt must be below 100). These
codes exceed the old run-number-only scheme. Local builds use `0.6.0+local`, code 1000000 and,
without the project key, a debug signature. They cannot replace a signed published APK in place;
a local release update requires the same key and a higher code. `tools/app_release.py` reads
Gradle's actual `output-metadata.json`, publishes a uniquely named APK and `app/version.json`
with its size, SHA-256 and revision. Old codes cannot be republished.

## On every push

A push to `main` or manual `.github/workflows/pages.yml` run:

1. Checks versions, upgrade ordering, translations and the generated `PortalPage.h`.
2. Builds and packages all 15 targets, all nRF52 language images, ESP32 installer language markers
   and community-board factory images.
3. Generates site screenshots in every language; failure now blocks publication.
4. Tests Android, builds the APK with this commit's web interface and signs it with the project
   key from secrets. Missing secrets fail the release instead of publishing a debug-signed APK.
5. Assembles the site, chess page, ZIP packages, installer manifests, `release.json`, APK and its
   update metadata. Checks hashes, the complete board list, consistent revisions and no Android downgrade.
6. Deploys the complete artifact to GitHub Pages only after every required job succeeds and while
   the commit is still the head of `main`. Failure leaves the previously published site available.

The site's `release.json` ties versions to the source revision and board image hashes. After
publishing, compare it with the pushed commit and `app/version.json` with the APK. Passing CI is
not hardware verification; record actual board results separately in `docs/verification.md`.

A push does not flash boards or install an APK on a phone. Update through the site/app/USB;
Android offers its update and opens the system installer. Firmware OTA is not implemented.
Documentation and `AGENTS.md` are edited before the commit. CI validates and publishes prepared
content; it does not invent hardware reports or update project instructions automatically.
