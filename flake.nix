{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs-wayland.url = "github:nix-community/nixpkgs-wayland";
    chaotic-cx.url = "github:chaotic-cx/nyx";
  };

  outputs = { self, nixpkgs, flake-utils, nixpkgs-wayland, chaotic-cx }:
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
          nixpkgs-wayland.packages.${system}.wlroots
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
