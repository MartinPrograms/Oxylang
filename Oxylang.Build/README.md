# Oxylang

To create a new project create a new directory and put a .toml file in there with the following fields: 

```toml
[project]
name = "Pong"
version = "0.1.0"
output = "bin/Pong"
type = "executable"
sources = ["src/**/*.oxy"]

[modules]
std = "${OXYLIB}/std/std.oxy" # Linking the standard library

[links]
libs = ["m", "raylib"]
```

Everything after the `[project]` header is optional.
`${OXYLIB}` is an environment variable, and should be set to `./liboxy/` but as a full path.