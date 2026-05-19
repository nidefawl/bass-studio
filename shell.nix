# Development environment. Uses sibling ../daw-deps when present.
(import ./default.nix {
  localDawDeps =
    let
      sibling = ../daw-deps;
    in
    if builtins.pathExists sibling then sibling else null;
}).devShell
