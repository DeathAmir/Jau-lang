# Jau packages

`jaupm` is written in Jau (`tools/jaupm.jau`). The native runtime only exposes primitive filesystem/network operations.

Default package layout:

```text
$JAU_HOME/
  packages/
    hello/
      main.jau
      package.meta
```

Import a package with `import "pkg:hello"`. A submodule can be imported with `import "pkg:hello/submodule.jau"`.

Commands:

```text
jaupm init NAME
jaupm install NAME [URL]
jaupm install-manifest URL
jaupm remove NAME
jaupm list
jaupm where NAME
```

If URL is omitted, JauPM reads `JAU_REGISTRY` and resolves `JAU_REGISTRY/NAME/main.jau`. A remote manifest for `install-manifest` must contain at least `name="..."` and `source="..."`.
