{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "awm";
          version = "0.1.0";
          src = self;
          outputs = [ "out" ];
          enableParallelBuilding = true;
          nativeBuildInputs = with pkgs; [
            meson
            ninja
            wayland-scanner
            pixman
            wayland
            wayland-protocols
            (wlroots.overrideAttrs (old: {
              src = pkgs.fetchgit {
                url = "https://gitlab.freedesktop.org/wlroots/wlroots.git";
                rev = "a64e1a58b19cc63a76830145a51375890fe66308";
                hash = "sha256-UeYTOOpURbw7g8y59iQYkTEXLc3Q4dKFM7FnUKD8F0s=";
              };
            }))
            pkg-config
            libxkbcommon
            xorg.libxcb
            egl-wayland
            libGL
            libinput
            xorg.xcbutilwm
          ];
        };
      }
    );
}
