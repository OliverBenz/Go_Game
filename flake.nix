{
  description = "Go_Game development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = [
          pkgs.gcc
          pkgs.cmake
          pkgs.SDL2
          pkgs.SDL2_image
          pkgs.glfw
          pkgs.opencv
          pkgs.zlib
          pkgs.libpng
          pkgs.libjpeg
          pkgs.libtiff
          pkgs.libwebp
        ];

        shellHook = ''
          echo "Dev shell ready. Use 'i-go-ui' to run the application on an intel card or n-go-ui to run the application on an nvidia card."

          alias n-go-ui='nixGLNvidia-580.82.09 ./out/bin/goUi'
          alias i-go-ui='nixGLIntel ./out/bin/goUi'
        '';
      };
    };
}
