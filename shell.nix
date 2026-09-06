{ pkgs ? import <nixpkgs> {} }:

let
  mingw32 = pkgs.pkgsCross.mingw32;
in
pkgs.mkShell {
  buildInputs = [
    # 32-bit MinGW cross-compiler
    mingw32.buildPackages.gcc
    mingw32.buildPackages.binutils

    # Build tools
    pkgs.gnumake
    pkgs.cmake
    pkgs.ninja

    # Wine (supports 32-bit Windows PE)
    pkgs.wineWow64Packages.stagingFull

    # Python and reverse-engineering utilities
    (pkgs.python3.withPackages (ps: with ps; [
      pyelftools
      pefile
      capstone
    ]))

    # Git
    pkgs.git
  ];

  shellHook = ''
    export CC=i686-w64-mingw32-gcc
    export CXX=i686-w64-mingw32-g++
    export WINEDLLOVERRIDES="dinput8=n,b"
  '';
}
