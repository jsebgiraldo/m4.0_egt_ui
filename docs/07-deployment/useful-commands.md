# Comandos Útiles — m4-egt-app

## Build Local (simulador x86)

```bash
# Compilar con el contenedor de desarrollo egt-app-dev
# Usa build-docker/ para evitar conflicto de CMakeCache con builds nativos
docker run --rm -v "$(pwd)":/app -w /app egt-app-dev \
  bash -c "mkdir -p build-docker && cd build-docker && cmake .. && make -j\$(nproc)"

# Correr el simulador (WSLg/X11)
docker run --rm -v "$(pwd)":/app -w /app/build-docker \
  -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
  -e EGT_BACKEND=x11 -e EGT_SCREEN_SIZE=800x480 \
  egt-app-dev ./egt-app
```

## Cross-Compilación para SAMA5D27

### Copiar fuentes al contenedor Yocto

```bash
docker start suntek-build

# Copiar src, CMakeLists.txt y assets al árbol de trabajo de Yocto
YOCTO_GIT=/opt/yocto/tmp/work/cortexa5t2hf-neon-vfpv4-poky-linux-gnueabi/my-egt-app/1.0/git
docker cp src/.   suntek-build:$YOCTO_GIT/src/
docker cp CMakeLists.txt suntek-build:$YOCTO_GIT/CMakeLists.txt
docker cp assets/. suntek-build:$YOCTO_GIT/assets/
```

### Compilar con contenedor ligero (evita bitbake OOM)

```bash
# Crear contenedor ligero sin el entrypoint pesado
docker run --rm -d --name suntek-ninja \
  -v yocto-build-cache:/opt/yocto/tmp \
  --entrypoint bash \
  suntek-yocto:latest \
  -c "sleep 600"

# Compilar con ninja
docker exec suntek-ninja bash -c "
  WORKDIR=/opt/yocto/tmp/work/cortexa5t2hf-neon-vfpv4-poky-linux-gnueabi/my-egt-app/1.0
  export PATH=\$WORKDIR/recipe-sysroot-native/usr/bin/arm-poky-linux-gnueabi:\$WORKDIR/recipe-sysroot-native/usr/bin:/opt/yocto/tmp/sysroots-components/x86_64/ninja-native/usr/bin:/opt/yocto/tmp/sysroots-components/x86_64/cmake-native/usr/bin:\$PATH
  cd \$WORKDIR/build
  ninja -j1
"

# Extraer binario
docker cp suntek-ninja:/opt/yocto/tmp/work/cortexa5t2hf-neon-vfpv4-poky-linux-gnueabi/my-egt-app/1.0/build/egt-app /tmp/egt-deploy/egt-app

# Limpiar
docker rm -f suntek-ninja
```

### Si se necesita re-cmake (ej: archivos nuevos)

```bash
docker exec suntek-ninja bash -c "
  WORKDIR=/opt/yocto/tmp/work/cortexa5t2hf-neon-vfpv4-poky-linux-gnueabi/my-egt-app/1.0
  export PATH=\$WORKDIR/recipe-sysroot-native/usr/bin/arm-poky-linux-gnueabi:\$WORKDIR/recipe-sysroot-native/usr/bin:/opt/yocto/tmp/sysroots-components/x86_64/ninja-native/usr/bin:/opt/yocto/tmp/sysroots-components/x86_64/cmake-native/usr/bin:\$PATH
  cd \$WORKDIR/build
  cmake -G Ninja \$WORKDIR/git \
    -DCMAKE_SYSROOT=\$WORKDIR/recipe-sysroot \
    -DCMAKE_C_COMPILER=arm-poky-linux-gnueabi-gcc \
    -DCMAKE_CXX_COMPILER=arm-poky-linux-gnueabi-g++
  ninja -j1
"
```

## Deploy al Target (SAMA5D27)

```bash
TARGET=root@192.168.1.125

# Parar servicio, subir binario, reiniciar
ssh $TARGET "systemctl stop egtapp.service"
scp /tmp/egt-deploy/egt-app $TARGET:/usr/bin/egt-app
ssh $TARGET "systemctl start egtapp.service"

# Verificar
ssh $TARGET "systemctl status egtapp.service | head -8"
```

## Target — Comandos Útiles

```bash
TARGET=root@192.168.1.125

# Ver estado del servicio
ssh $TARGET "systemctl status egtapp.service"

# Ver logs de la app
ssh $TARGET "journalctl -u egtapp.service --no-pager -n 30"

# Parar/Iniciar servicio
ssh $TARGET "systemctl stop egtapp.service"
ssh $TARGET "systemctl start egtapp.service"

# Verificar qué binario está corriendo
ssh $TARGET "md5sum /usr/bin/egt-app"

# Ver procesos que usan DRM (si hay crash "unable to create primary plane")
ssh $TARGET "ps aux | grep -E 'drm|egt' | grep -v grep"

# Limpiar credenciales WiFi y reiniciar app (pide WiFi de nuevo)
ssh $TARGET "reset-wifi.sh"

# Capturar screenshot del display (DRM framebuffer → PNG)
ssh $TARGET "python3 /usr/bin/capture-screen.py /tmp/screen.ppm"
scp $TARGET:/tmp/screen.ppm /tmp/target-screen.ppm
convert /tmp/target-screen.ppm /tmp/target-screen.png
# O usar el script wrapper:
# ./scripts/screenshot.sh /tmp/target-screen.png
```

## Embeber Assets

Cuando se agrega un nuevo asset (ej: nuevo PNG):

```bash
# Generar header con datos embebidos
xxd -i assets/image/NuevoAsset.png >> src/generated/embedded_assets.h

# Agregar #pragma once y const manualmente si se regenera todo el archivo
xxd -i assets/image/Lice-logo.png > src/generated/embedded_assets.h
# Editar: agregar #pragma once al inicio, cambiar a static const

# Usar en código:
# Image(assets_image_NuevoAsset_png, assets_image_NuevoAsset_png_len)
```

## Notas

- **El contenedor `suntek-build` tiene un entrypoint pesado** que ejecuta `chown -R` + `bitbake suntek-image` al iniciar. Usar `suntek-ninja` (contenedor ligero) para compilar rápido.
- **El servicio `egtapp.service`** tiene `Restart=on-failure` — siempre usar `systemctl stop/start`, nunca `kill` directo.
- **Los assets están embebidos en el binario** (via `src/generated/embedded_assets.h`), no se necesitan archivos externos en el target.
- **Las imágenes deben ser PNG real** (no JPEG renombrado a .png). Si EGT lanza `read past end of data stream`, verificar con `file imagen.png` y convertir con `convert imagen.png imagen-real.png`.
- **Target**: SAMA5D27 WLSOM1 EK SD, IP 192.168.1.125, usuario root sin contraseña.
