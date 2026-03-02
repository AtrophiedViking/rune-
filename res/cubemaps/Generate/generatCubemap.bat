@echo off
setlocal

REM ============================================================
REM 1. Create main cubemap (mipmapped)
REM ============================================================
echo Creating main cubemap...
toktx --cubemap --t2 --genmipmap env_cubemap.ktx2 ^
    px.png nx.png py.png ny.png pz.png nz.png

REM ============================================================
REM 2. Create irradiance map (blurred, low-res)
REM ============================================================
echo Creating irradiance map...
toktx --cubemap --t2 --genmipmap --filter lanczos3 --resize 32x32 env_irradiance.ktx2 ^
    px.png nx.png py.png ny.png pz.png nz.png

REM ============================================================
REM 3. Create specular GGX prefiltered map (IBLBaker)
REM ============================================================
echo Creating specular GGX prefiltered map with IBLBaker...
IBLBaker.exe --input skybox.hdr --output ibl_out --specular --ibl-samples 1024

REM IBLBaker outputs:
REM   ibl_out/specular_ibl.ktx        (specular GGX)
REM   ibl_out/irradiance_ibl.ktx      (irradiance)
REM   ibl_out/brdf_lut.png            (BRDF LUT)

REM Convert specular to KTX2
echo Converting specular GGX map to KTX2...
toktx --t2 env_specular.ktx2 ibl_out/specular_ibl.ktx

REM ============================================================
REM 4. Create BRDF LUT (use IBLBaker output)
REM ============================================================
echo Creating BRDF LUT...
toktx --t2 brdf_lut.ktx2 brdf_lut.png

echo Done.
pause
