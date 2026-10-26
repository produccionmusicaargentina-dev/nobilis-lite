#pragma once

// ---------------------------------------------------------------------------
//  Licencias de Nobilis
//
//  kLicensePublicKeyHex: la clave PÚBLICA con la que el plugin verifica las licencias.
//  Se genera en la web: /admin → HERRAMIENTAS → "Generar par de claves".
//  Reemplazá la línea entera por la que te da esa pantalla (64 caracteres hexadecimales).
//  La clave pública no es secreta: puede estar en GitHub sin problema.
// ---------------------------------------------------------------------------
static constexpr const char* kLicensePublicKeyHex = "bfe4c8fcdde54a6f339435184669ac2ff12e64b6bdea1ad1926c31ca1dd58d92";

// Dirección de la web (sin barra al final). Se puede cambiar al compilar con -DNOBILIS_SERVER_URL=...
#ifndef NOBILIS_SERVER_URL
 #define NOBILIS_SERVER_URL "https://nobilis.ar"
#endif
static constexpr const char* kLicenseServer = NOBILIS_SERVER_URL;
