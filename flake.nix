{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs-wayland.url = "github:nix-community/nixpkgs-wayland";
  };

  outputs = { self, nixpkgs, flake-utils, nixpkgs-wayland }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        dependencies = with pkgs; [
          meson
          ninja
          wayland-scanner
          pixman
          wayland
          wayland-protocols
          pkg-config
          libxkbcommon
          xorg.libxcb
          egl-wayland
          libGL
          libinput
          xorg.xcbutilwm
        ] ++ [
          nixpkgs-wayland.packages.${system}.wlroots
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
