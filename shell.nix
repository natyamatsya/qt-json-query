{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    qt6.full
    clang
    gcc
    cmake
    ninja
    pkg-config
    # Add other development tools as needed
  ];
}