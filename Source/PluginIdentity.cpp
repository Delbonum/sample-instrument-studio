#include "PluginIdentity.h"

#include <string.h>

/*
    Der Datenblock, den der Export ersetzt. Aufbau:

        SIS-IDENTITY-v1|<4-stelliger Code>|<Anzeigename>

    Die Marke am Anfang macht die Stelle im Binary eindeutig auffindbar. Der Block ist
    bewusst groß genug für lange Instrumentnamen und `volatile`, damit der Compiler die
    Werte nicht als Konstanten einsetzt.
*/
#define SIS_IDENTITY_MARKER "SIS-IDENTITY-v1|"
#define SIS_IDENTITY_BLOCK_SIZE 192

volatile char sisIdentityBlock[SIS_IDENTITY_BLOCK_SIZE] =
    SIS_IDENTITY_MARKER "Sist|Sample Instrument Studio";

static char cachedName[SIS_IDENTITY_BLOCK_SIZE];
static char cachedCode[8];
static int identityParsed = 0;

/** Kopiert höchstens `max - 1` Zeichen und schließt mit einer Null ab. */
static void copyText (char* destination, const char* source, size_t max)
{
    size_t i = 0;

    while (i + 1 < max && source[i] != 0)
    {
        destination[i] = source[i];
        ++i;
    }

    destination[i] = 0;
}

static void parseIdentity (void)
{
    char block[SIS_IDENTITY_BLOCK_SIZE];
    const char* code;
    const char* name;
    size_t i;

    if (identityParsed)
        return;

    identityParsed = 1;
    copyText (cachedCode, "Sist", sizeof (cachedCode));
    copyText (cachedName, "Sample Instrument Studio", sizeof (cachedName));

    /* volatile byteweise kopieren, damit der Compiler nichts wegoptimiert */
    for (i = 0; i < SIS_IDENTITY_BLOCK_SIZE; ++i)
        block[i] = sisIdentityBlock[i];

    block[SIS_IDENTITY_BLOCK_SIZE - 1] = 0;

    if (strncmp (block, SIS_IDENTITY_MARKER, strlen (SIS_IDENTITY_MARKER)) != 0)
        return;

    code = block + strlen (SIS_IDENTITY_MARKER);
    name = strchr (code, '|');

    if (name == NULL || (size_t) (name - code) != 4)
        return;

    memcpy (cachedCode, code, 4);
    cachedCode[4] = 0;

    ++name;

    if (*name != 0)
        copyText (cachedName, name, sizeof (cachedName));
}

const char* sisExportedPluginName (void)
{
    parseIdentity();
    return cachedName;
}

unsigned int sisExportedPluginCode (void)
{
    parseIdentity();

    return ((unsigned int) (unsigned char) cachedCode[0] << 24)
         | ((unsigned int) (unsigned char) cachedCode[1] << 16)
         | ((unsigned int) (unsigned char) cachedCode[2] << 8)
         | ((unsigned int) (unsigned char) cachedCode[3]);
}
