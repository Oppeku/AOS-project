# Uni application packages

Uni is the AOS local application installer. It owns installed application state;
Acur is its catalog and download frontend, and MUI Software is its visual
frontend.

## Commands

```sh
uni inspect FILE
uni info FILE
uni info PACKAGE
uni install FILE
uni list
uni run PACKAGE [--experimental] [ARGS...]
uni remove PACKAGE
```

Installed files live below `/main/Applications/PACKAGE`. Metadata is stored in
`/main/Applications/installed.db`, with one generated `.uni-manifest` per
package. Downloads are kept in `/main/Downloads`.

## Supported containers

- Debian `.deb`: ar format with `debian-binary`, `control.tar`, and `data.tar`
- `.ainstall`: AOS package metadata plus a tar payload
- `.AppImage`: an x86_64 ELF image copied as one package entry

Tar members can be uncompressed, gzip-compressed, or XZ-compressed. The XZ
decoder accepts no check, CRC32, and CRC64 streams, including the x86 BCJ filter.
Zstandard-compressed Debian members are not supported yet.

Extraction rejects absolute paths, parent traversal, overlong paths, malformed
archives, oversized payloads, and unsupported package architectures. Uni writes
files only inside the package directory and records installation only after
extraction and manifest creation succeed.

## Executable compatibility

Container support and executable compatibility are separate:

- `aos-native`: AOS ELF64 executable; supported by `uni run` and MUI Open
- `linux-static`: recognized but experimental; launch requires
  `uni run PACKAGE --experimental`
- `linux-dynamic`: installable for inspection, but cannot run until AOS has the
  required Linux ABI, dynamic loader, shared libraries, and display integration
- data-only package: installable, with no Open action

Therefore, support for `.deb` does not imply support for every Debian
application. Packages must contain binaries compatible with the current AOS
userspace contract.

## Acur integration

The catalog format is:

```txt
http://host/path/application.deb --package-name --sha256 64_hex_characters
```

`acur fetch` downloads and verifies into `/main/Downloads`. `acur install`
performs the same verification and then executes `uni install`. `acur installed`
and `acur remove` execute the corresponding Uni command, so there is no second
installation database.

Acur currently supports direct HTTP URLs and HTTP redirects. HTTPS catalog
downloads are pending TLS support.

## MUI integration

- Opening a `.deb`, `.ainstall`, or `.AppImage` in Files starts Uni.
- Package work runs in a child process, so the desktop remains responsive.
- Software reads the persistent database and supports kind filters, Open,
  Remove, Refresh, and a Downloads shortcut.
- Open is offered only for AOS-native executable packages.

## Regression tests

Run the host-side codec checks with:

```sh
make test-uni-codecs
```

The target verifies gzip and XZ decoding against the README payload, covers XZ
CRC64, CRC32, no-check, and x86-filter streams, and checks that truncated streams
are rejected.
