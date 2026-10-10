# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
{
  description = "FTZ reproducible floating-point arithmetic";

  inputs = {
    native.url = "git+https://github.com/ekmett/native?ref=main&shallow=1";
    nixpkgs.follows = "native/nixpkgs";
  };

  outputs = { self, native, nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          dependency = native.packages.${system}.native;
          ftz = pkgs.llvmPackages_23.stdenv.mkDerivation {
            pname = "ftz";
            version = "0.0.1";
            src = pkgs.lib.fileset.toSource {
              root = ./.;
              fileset = pkgs.lib.fileset.unions [
                ./CMakeLists.txt ./LICENSE.md
                ./src ./cmake
                ./tests/api ./tests/ftz_module/native_api.cc
              ];
            };
            nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.llvmPackages_23.clang-tools ];
            propagatedBuildInputs = [ dependency ];
            cmakeFlags = [ "-DFTZ_BUILD_TESTS=OFF" ];
            doInstallCheck = true;
            installCheckPhase = ''
              runHook preInstallCheck
              cmake -S "$src/tests/api" -B consumer -G Ninja \
                -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$out;${dependency}"
              cmake --build consumer --parallel "$NIX_BUILD_CORES"
              ctest --test-dir consumer --output-on-failure
              runHook postInstallCheck
            '';
            meta = {
              description = "Fast, reproducible floating-point arithmetic on Native";
              homepage = "https://github.com/ekmett/ftz";
              license = with pkgs.lib.licenses; [ bsd2 asl20 ];
              platforms = systems;
            };
          };
        in { inherit ftz; default = ftz; });
      checks = forAllSystems (system: { inherit (self.packages.${system}) ftz; });
      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = (pkgs.mkShell.override { stdenv = pkgs.llvmPackages_23.stdenv; }) {
            inputsFrom = [ self.packages.${system}.ftz ];
          };
        });
    };
}
