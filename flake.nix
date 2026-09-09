{
  description = "Go_Game development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default =
        let
          # NOTE: OpenCV5 is not available in nixpkgs yet. So we cannot use nix flake.
          # We could locally compile but this takes too long. Just install system wide for now.
          # We will fix this in the future.
          opencvWithGui = pkgs.opencv4.override {
            enableGtk3 = true;
          };
          runtimeLibraries = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtwayland
            pkgs.glfw
            opencvWithGui
            pkgs.gtk3
            pkgs.zlib
            pkgs.libpng
            pkgs.libjpeg
            pkgs.libtiff
            pkgs.libwebp
            pkgs.libglvnd
            pkgs.stdenv.cc.cc.lib
          ];
        in pkgs.mkShell {
        packages = [
          pkgs.gcc
          pkgs.cmake
          pkgs.pkg-config
        ] ++ runtimeLibraries;

        shellHook = ''
          export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath runtimeLibraries}:$LD_LIBRARY_PATH"

          echo "Dev shell ready. Use 'go-ui' to run the GUI application on x64 Debug."

          alias go-ui='nixGL  ./out/bin/x64/Debug/goUi'
        '';
      };
    };
}
