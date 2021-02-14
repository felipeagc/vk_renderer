{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
	buildInputs = with pkgs; [
		gcc
		cmake
		ninja
		xorg.libX11
		xorg.libXau
		xorg.libXcursor
		xorg.libXrandr
		xorg.libXinerama
		xorg.libXi
		xorg.libXext
		libGL
		spirv-tools
		vulkan-loader
		vulkan-extension-layer
		vulkan-validation-layers
		directx-shader-compiler
		python3
		renderdoc
	];

	LD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib";
	QT_QPA_PLATFORM="xcb";
}

