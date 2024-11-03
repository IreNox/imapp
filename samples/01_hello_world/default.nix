with (import <nixpkgs> {}) ;
mkShell {
  hardeningDisable = [ "fortify" ];
  buildInputs = [
    gnumake
    pkg-config
    gcc
    glibc_multi
    bear
    libGLU
    libGL.dev
    dbus.dev
    #libxkbcommon.dev    
    #wayland.dev
    #wayland-scanner
    xorg.libX11
    xorg.libXext
  ];
}