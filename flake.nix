{
	description = "PCRE2 fork build and upstream C tests";
	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
	outputs = { self, nixpkgs }:
		let
			systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
			forSystems = nixpkgs.lib.genAttrs systems;
			package = system: checked:
				let pkgs = nixpkgs.legacyPackages.${system};
				in pkgs.stdenv.mkDerivation {
					pname = "pcre2-dfa-check";
					version = "10.48-dev";
					src = self;
					strictDeps = true;
					nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
					postPatch = ''
						substituteInPlace RunGrepTest --replace-fail /bin/echo ${pkgs.coreutils}/bin/echo
					'';
					cmakeBuildType = "Release";
					cmakeBuildDir = ".cmake-build";
					cmakeFlags = [
						"-DPCRE2_BUILD_PCRE2_16=ON" "-DPCRE2_BUILD_PCRE2_32=ON" "-DPCRE2_SUPPORT_JIT=OFF"
						"-DCMAKE_INSTALL_LIBDIR=lib" "-DCMAKE_INSTALL_INCLUDEDIR=include" "-DCMAKE_INSTALL_BINDIR=bin"
					];
					doCheck = checked;
					checkPhase = ''
						runHook preCheck
						ctest --output-on-failure
						runHook postCheck
					'';
				};
		in {
			packages = forSystems (system: { default = package system false; });
			checks = forSystems (system: {
				test = package system true;
				shared = (package system true).overrideAttrs (old: {
					cmakeFlags = old.cmakeFlags ++ [ "-DBUILD_STATIC_LIBS=OFF" "-DBUILD_SHARED_LIBS=ON" ];
				});
				# Zig build graph and capture-history binding tests at every code-unit width.
				zig =
					let pkgs = nixpkgs.legacyPackages.${system};
					in pkgs.stdenvNoCC.mkDerivation {
						pname = "pcre2-zig-check";
						version = "10.48-dev";
						src = self;
						strictDeps = true;
						nativeBuildInputs = [ pkgs.zig ];
						dontConfigure = true;
						dontInstall = true;
						dontFixup = true;
						buildPhase = ''
							runHook preBuild
							export HOME=$TMPDIR
							export ZIG_GLOBAL_CACHE_DIR=$TMPDIR/zig-global
							for width in 8 16 32; do
								zig build test -Dcode-unit-width=$width --summary all --cache-dir "$TMPDIR/zig-cache" --prefix "$TMPDIR/out-$width"
							done
							(cd experiments/dna_repeats && zig build test --summary all --cache-dir "$TMPDIR/zig-cache-dna")
							mkdir -p $out
							echo "zig tests passed" > $out/result
							runHook postBuild
						'';
					};
			});
			devShells = forSystems (system:
				let pkgs = nixpkgs.legacyPackages.${system};
				in { default = pkgs.mkShell { packages = [ pkgs.cmake pkgs.ninja pkgs.pkg-config pkgs.zig pkgs.hyperfine ]; }; });
		};
}
