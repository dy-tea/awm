{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    chaotic-cx.url = "github:chaotic-cx/nyx";
    self.submodules = true;
  };

  outputs = { self, nixpkgs, flake-utils, chaotic-cx }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        dependencies = with pkgs; [
          meson
          ninja
          cmake
          wayland-scanner
          pixman
          wayland
          xwayland
          pkg-config
          libxkbcommon
          xorg.libxcb
          egl-wayland
          libGL
          libinput
          xorg.xcbutilwm
        ] ++ [
          chaotic-cx.packages.${system}.wlroots_git
          chaotic-cx.packages.${system}.wayland-protocols_git
        ];
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "awm";
          version = "0.1.0";
          src = self;
          outputs = [ "out" ];
          enableParallelBuilding = true;
          nativeBuildInputs = dependencies;

          meta = with pkgs.lib; {
            description = "a wm";
            platforms = [ "x86_64-linux" ];
          };
        };

        devShell = pkgs.mkShell {
          nativeBuildInputs = dependencies;
        };
      }
    );
}
