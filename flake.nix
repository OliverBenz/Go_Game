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
      devShells.${system}.default = pkgs.mkShell {
        packages = [
          pkgs.gcc
          pkgs.cmake
          pkgs.qt6.qtbase
          pkgs.qt6.qtwayland
          pkgs.glfw
          pkgs.opencv
          pkgs.zlib
          pkgs.libpng
          pkgs.libjpeg
          pkgs.libtiff
          pkgs.libwebp
        ];

        shellHook = ''
          echo "Dev shell ready. Use 'go-ui' to run the GUI application on x64 Debug."

          alias go-ui='nixGL  ./out/bin/x64/Debug/goUi'
        '';
      };
    };
}
