# Byte Shift Codec (BSC)

Cette lib C implemente un decalage de type Cesar, mais applique par blocs avec
une cle compacte. Mon idee: rester simple, deterministe, et donner un format de
cle facile a transmettre (un tableau d octets brut). Je voulais aussi un mode
"rounds" pour repeter l operation sans complexifier le format.

## Resume rapide

- Entree = buffer d octets.
- Cle = `key[0]` pour la taille de bloc `XX`, puis une liste `YY[]` qui se repete.
- Si la taille n est pas multiple de `XX`, on ajoute du padding de valeur `pad_len` (style PKCS#7).
- Le pad_len est stocke en suffixe (1 octet, non encode).
- Encodage: ajout du shift par bloc (mod 256).
- Decodage: soustraction du shift par bloc (mod 256).
- Si rounds % 256 == 0, on force rounds_mod = 1 pour eviter un shift nul (warning).

## Format de cle

- `key[0]` = `XX` (taille de bloc), doit etre dans 1..255.
- `key[1..]` = `YY[]`, une valeur par bloc, type `uint8` (0..255).
- `yy_len = key_len - 1`.
- Si on a plus de blocs que de `YY`, on boucle (cycle).

Exemple:

- `key = { 0x04, 0x05, 0xF0 }`
- `XX = 4`
- `YY = [0x05, 0xF0]`
- Les shifts par bloc sont: 0x05, 0xF0, 0x05, 0xF0, ...

## Regle de transformation

On coupe les donnees en blocs de `XX` octets. Pour chaque bloc `b`:

- `shift = (YY[b % yy_len] * rounds_eff) mod 256`
- Encodage: `out = (in + shift) mod 256`
- Decodage: `out = (in - shift) mod 256`

`rounds_eff` est `rounds` si `rounds != 0`, sinon la valeur par defaut.
Si `rounds_eff % 256 == 0`, on force `rounds_mod = 1` pour eviter un shift nul.

## Padding (choix volontaire)

Si la taille n est pas multiple de `XX`, on ajoute des octets de valeur
`pad_len` (style PKCS#7) pour atteindre un multiple. Le nombre d octets ajoutes
(`pad_len`) est stocke en suffixe (1 octet) et n est pas encode. Si la taille
est deja multiple de `XX`, `pad_len = 0` et aucun bloc en plus n est ajoute.

- `pad_len = (XX - (in_len % XX)) % XX`
- `padded_len = in_len + pad_len`
- Le flux encode contient `padded_len` octets encodes, puis 1 octet de meta.
- Au decodage, on lit `pad_len` a la fin et on retire ces octets.

## API C

```c
int bsc_encode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written);

int bsc_decode(const uint8_t *in,
               size_t in_len,
               const uint8_t *key,
               size_t key_len,
               uint32_t rounds,
               uint8_t *out,
               size_t out_len,
               size_t *out_written);
```

- `rounds`: si 0, on utilise `BYTE_SHIFT_DEFAULT_ROUNDS` (10).
- `out_written`: nombre d octets produits.
- Encodage: `out_written = padded_len + 1` (payload encode + pad_len).
- Decodage: `out_written = in_len` original (apres depadding).
- Entree et sortie sont deux buffers distincts.
- Aucune allocation dynamique: pas de malloc/free dans la lib.
- La cle est geree par l appelant (la lib ne la conserve pas, ne l efface pas).
- Encodage: `out_len >= in_len + pad_len + 1` avec `pad_len = (XX - (in_len % XX)) % XX`.
- Decodage: `out_len >= (in_len - 1) - pad_len` avec `pad_len = in[in_len - 1]`.
- Sur succes, retour = `BSC_OK` ou `BSC_WARN_ROUNDS_CLAMPED`.
- Sur erreur, `out_written` est mis a 0 si fourni.

## Erreurs

- `BSC_OK` = succes
- `BSC_WARN_ROUNDS_CLAMPED` = warning (rounds multiple de 256 force a 1)
- `BSC_ERR_BAD_KEY` = cle invalide (`key_len < 2` ou `XX = 0`)
- `BSC_ERR_BAD_PARAM` = pointeurs invalides
- `BSC_ERR_OUT_TOO_SMALL` = buffer de sortie trop petit
- `BSC_ERR_LEN_MISMATCH` = flux encode invalide (payload pas multiple de `XX`)
- `BSC_ERR_BAD_PADDING` = `pad_len` invalide

## Exemple simple

```c
uint8_t input[] = { 0x01 };
uint8_t key[] = { 0x01, 0x01 };
uint8_t encoded[2] = { 0 };
size_t written = 0;

bsc_encode(input, 1, key, 2, 1, encoded, 2, &written);
// encoded = { 0x02, 0x00 }, written = 2
```

## Exemple avec cycles et padding

- `input` = 13 octets
- `XX = 4` donc `pad_len = 3`, `padded_len = 16`
- `YY = [0x05, 0xF0]` se repete sur les blocs

## Compilation

```sh
gcc -std=c11 -Wall -Wextra byte_shift_codec.c byte_shift_codec_example.c -o bsc_example
./bsc_example
```

## Limites et securite

Ce n est pas de la crypto. C est une obfuscation simple et reversible.
Si tu veux du vrai chiffrement, il faut un algo standard et audite.
