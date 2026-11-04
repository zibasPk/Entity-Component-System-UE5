# ECS C++

A C++ entity component system learning project, structured as a reusable library.

## Project layout

```text
include/
  ecs/        Public library headers and template definitions
src/          Compiled implementation and private headers
tests/        Automated library tests
examples/     Small applications demonstrating library usage
docs/         Design notes and API contracts
```

Public headers will be included as `ecs/<header>.hpp`. Keep application-specific
components and systems in examples or consuming applications.

When adding the build system, define a separate library target and link tests and
examples against it. Keep generated build files in `build/`. This scaffold does
not yet contain C++ sources or a build configuration.
