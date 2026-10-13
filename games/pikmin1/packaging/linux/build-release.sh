#!/usr/bin/env bash
# build-release.sh [--clean]
#
# Paquete de Linux de una release, en dos formatos con el mismo contenido:
#
#   packaging/linux/out/Pikmin-1-linux.tar.gz        carpeta para extraer
#   packaging/linux/out/Pikmin-1-x86_64.AppImage     un solo archivo, doble clic
#
# Dentro van tres ejecutables: Pikmin 1 (USA), Pikmin 1 PAL (Europa) y
# Pikmin 1 Launcher. El launcher instala solo el de la región del disco, con el
# nombre Pikmin 1, así que la carpeta instalada queda en dos archivos.
#
# Se compila en un contenedor Ubuntu 20.04 (podman) para que los ejecutables
# arranquen en cualquier distro con glibc 2.29 o posterior. SDL2 y el runtime
# de C++ van dentro; solo dependen de glibc, libGL y libX11.
#
# La versión sale de OPEN_NECTAR_VERSION en CMakeLists.txt (o de la variable
# de entorno del mismo nombre) y es la que el launcher compara con GitHub.
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${repo_root}/build-linux-release"
out_dir="${script_dir}/out"
package_dir="${out_dir}/Pikmin-1-linux"
appdir="${build_dir}/AppDir"
image="open-nectar-build"

if [[ "${1:-}" == "--clean" ]]; then
    rm -rf "${build_dir}" "${out_dir}"
fi

version="${OPEN_NECTAR_VERSION:-$(sed -n 's/.*set(OPEN_NECTAR_VERSION "\([^"]*\)".*/\1/p' "${repo_root}/CMakeLists.txt")}"
if [[ -z "${version}" ]]; then
    printf 'No se encontró OPEN_NECTAR_VERSION en CMakeLists.txt\n' >&2
    exit 1
fi
if ! command -v podman >/dev/null 2>&1; then
    printf 'Hace falta podman: sudo apt install podman\n' >&2
    exit 1
fi

printf '[1/4] Imagen de compilación (Ubuntu 20.04)...\n'
podman build -q -t "${image}" -f "${script_dir}/Containerfile" "${script_dir}" >/dev/null

printf '[2/4] Compilando Pikmin 1 %s en el contenedor...\n' "${version}"
podman run --rm --userns=keep-id -v "${repo_root}":/src -e OPEN_NECTAR_VERSION="${version}" \
    "${image}" bash /src/packaging/linux/container-build.sh

usa="${build_dir}/usa/bin/Pikmin 1"
pal="${build_dir}/pal/bin/Pikmin 1 PAL"
launcher="${build_dir}/usa/bin/Pikmin 1 Launcher"
for required in "${usa}" "${pal}" "${launcher}"; do
    [[ -f "${required}" ]] || { printf 'No se generó %s\n' "${required}" >&2; exit 1; }
done
if cmp -s "${usa}" "${pal}"; then
    printf 'Pikmin 1 y Pikmin 1 PAL son el mismo archivo: la build PAL no se hizo.\n' >&2
    exit 1
fi

# Solo glibc, libGL y libX11: cualquier otra dependencia es un error de empaquetado.
allowed='^(libc|libm|libdl|libpthread|librt|ld-linux-x86-64)\.so|^libGL\.so\.1$|^libX11\.so\.6$'
for exe in "${usa}" "${pal}" "${launcher}"; do
    while read -r lib; do
        if ! printf '%s\n' "${lib}" | grep -Eq "${allowed}"; then
            printf '%s depende de %s, que no está en todos los sistemas. Revisa PIKMIN_STATIC_RUNTIME.\n' "${exe}" "${lib}" >&2
            exit 1
        fi
    done < <(readelf -d "${exe}" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p')
done

printf '[3/4] Montando Pikmin-1-linux.tar.gz...\n'
rm -rf "${package_dir}"
mkdir -p "${package_dir}"
install -m 755 "${usa}" "${package_dir}/Pikmin 1"
install -m 755 "${pal}" "${package_dir}/Pikmin 1 PAL"
install -m 755 "${launcher}" "${package_dir}/Pikmin 1 Launcher"
strip "${package_dir}/Pikmin 1" "${package_dir}/Pikmin 1 PAL" "${package_dir}/Pikmin 1 Launcher" 2>/dev/null || true
cp "${script_dir}/README.txt" "${script_dir}/LEEME.txt" "${package_dir}/"
cp "${repo_root}/packaging/icon/open_nectar.png" "${package_dir}/open_nectar.png"
tar -C "${out_dir}" -czf "${out_dir}/Pikmin-1-linux.tar.gz" Pikmin-1-linux

printf '[4/4] Montando Pikmin-1-x86_64.AppImage...\n'
rm -rf "${appdir}"
mkdir -p "${appdir}/usr/bin"
cp "${package_dir}/Pikmin 1" "${package_dir}/Pikmin 1 PAL" "${package_dir}/Pikmin 1 Launcher" "${appdir}/usr/bin/"
ln -s "usr/bin/Pikmin 1 Launcher" "${appdir}/AppRun"
cp "${repo_root}/packaging/icon/open_nectar.png" "${appdir}/open-nectar.png"
cat >"${appdir}/open-nectar.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Pikmin 1
Comment=Native Pikmin port: install, update and play
Exec="Pikmin 1 Launcher"
Icon=open-nectar
Terminal=false
Categories=Game;
DESKTOP
tool="${build_dir}/appimagetool-x86_64.AppImage"
if [[ ! -x "${tool}" ]]; then
    curl -fsSL -o "${tool}" \
        https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "${tool}"
fi
# Sin FUSE: appimagetool se extrae a sí mismo. El AppImage resultante usa el
# runtime estático nuevo, que tampoco necesita libfuse2 en el sistema.
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 VERSION="${version}" \
    "${tool}" --no-appstream "${appdir}" "${out_dir}/Pikmin-1-x86_64.AppImage" >/dev/null

printf '\nPikmin 1 %s listo para publicar (sube los dos al release %s de GitHub):\n' "${version}" "${version}"
printf '  %s\n' "${out_dir}/Pikmin-1-linux.tar.gz" "${out_dir}/Pikmin-1-x86_64.AppImage"
