@echo off
setlocal

echo Creating main cubemap...
toktx --cubemap --t2 --genmipmap env_cubemap.ktx2 ^
    px.png nx.png py.png ny.png pz.png nz.png

echo Creating irradiance map...
toktx --cubemap --t2 --genmipmap --filter lanczos3 --resize 32x32 env_irradiance.ktx2 ^
    px.png nx.png py.png ny.png pz.png nz.png

echo Creating prefiltered specular map...
toktx --cubemap --t2 --genmipmap --filter lanczos3 env_prefilter.ktx2 ^
    px.png nx.png py.png ny.png pz.png nz.png

echo Creating BRDF LUT...
toktx --t2 --genmipmap brdf_lut.ktx2 brdf_lut.png

echo Done.
pause
