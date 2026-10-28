#pragma once

// Escritor de .mod para los modelos de relleno de los tests de escenarios
// (issue #65). Es la pareja de parseMod: toma el Model que lee mod.h y escribe
// lo que BaseShape::read del juego (src/sysCommon/shapeBase.cpp) espera.
//
// Decisión de diseño: los modelos son RÍGIDOS, no con envolventes. Cada malla
// cuelga de su articulación (ModMesh::parentJoint = j) y su grupo de matrices
// lleva una sola dependencia, un ModVtxMatrix que apunta a esa articulación.
// No se escribe el bloque MatrixEnvelope.
//
// Lo que escribe, en este orden (el de writeMeshMod): 0x00 cabecera, 0x10
// posiciones, 0x11 normales, 0x18..0x1F UV que haya, 0x30 materiales, 0x40
// matrices de vértices, 0x50 mallas, 0x60 articulaciones, 0x61 nombres (solo
// si alguna articulación tiene), 0xFFFF fin. Cada bloque empieza alineado a 32
// y su largo cubre el relleno final: el juego ignora el largo de los bloques
// conocidos pero parseMod salta por él, así que tiene que ser exacto. Tras el
// bloque final va Model::trailingIni tal cual (texto collinfo, sin relleno,
// como los .mod del disco).
//
// Lo que NO escribe (devuelve error si el Model lo trae, para no perderlo en
// silencio): texturas, atributos de textura, materiales PVW o TEV, colores de
// vértice, envolventes y colisión de mapa (0x100, 0x110). El bloque 0xF00 del
// port tampoco se escribe: se usa la ruta de dibujo clásica.

#include "nectar_assets/mod.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nectar {

/// El .mod listo para guardar. false con el motivo si el Model no se puede
/// escribir (ver arriba) o rompe una regla del juego: articulación 0 raíz,
/// padre de cada articulación con índice menor, índices dentro de rango.
bool writeMod(const Model& model, std::vector<std::uint8_t>& out, std::string& error);

} // namespace nectar
