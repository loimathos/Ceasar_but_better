# Byte Shift Codec (BSC)

Cette lib C implemente un decalage de type Cesar, mais applique par blocs avec
une cle compacte. Mon idee: rester simple, deterministe, et donner un format de
cle facile a transmettre (un tableau d octets brut). Je voulais aussi un mode
"rounds" pour repeter l operation sans complexifier le format.

## Resume rapide

- Entree = buffer d octets.
- Cle = `key[0]` pour la taille de bloc `XX`, puis une liste `YY[]` qui se repete.
- On ne pad pas, le dernier bloc peut etre incomplet.
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

On coupe les donnees en blocs de `XX` octets (le dernier bloc peut etre partiel).
Pour chaque bloc `b`:

- `shift = (YY[b % yy_len] * rounds_eff) mod 256`
- Encodage: `out = (in + shift) mod 256`
- Decodage: `out = (in - shift) mod 256`

`rounds_eff` est `rounds` si `rounds != 0`, sinon la valeur par defaut.
Si `rounds_eff % 256 == 0`, on force `rounds_mod = 1` pour eviter un shift nul.

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
- Encodage: `out_written = in_len`.
- Decodage: `out_written = in_len`.
- Entree et sortie sont deux buffers distincts.
- Aucune allocation dynamique: pas de malloc/free dans la lib.
- La cle est geree par l appelant (la lib ne la conserve pas, ne l efface pas).
- Encodage: `out_len >= in_len`.
- Decodage: `out_len >= in_len`.
- Sur succes, retour = `BSC_OK` ou `BSC_WARN_ROUNDS_CLAMPED`.
- Sur erreur, `out_written` est mis a 0 si fourni.

## Erreurs

- `BSC_OK` = succes
- `BSC_WARN_ROUNDS_CLAMPED` = warning (rounds multiple de 256 force a 1)
- `BSC_ERR_BAD_KEY` = cle invalide (`key_len < 2` ou `XX = 0`)
- `BSC_ERR_BAD_PARAM` = pointeurs invalides
- `BSC_ERR_OUT_TOO_SMALL` = buffer de sortie trop petit

## Exemple simple

```c
uint8_t input[] = { 0x01 };
uint8_t key[] = { 0x01, 0x01 };
uint8_t encoded[2] = { 0 };
size_t written = 0;

bsc_encode(input, 1, key, 2, 1, encoded, 2, &written);
// encoded = { 0x02 }, written = 1
```

## Exemple avec cycles et bloc partiel

- `input` = 13 octets
- `YY = [0x05, 0xF0]` se repete sur les blocs

## Compilation

```sh
gcc -std=c11 -Wall -Wextra byte_shift_codec.c byte_shift_codec_example.c -o bsc_example
./bsc_example
```

## Limites et securite

Ce n est pas de la crypto. C est une obfuscation simple et reversible.
Si tu veux du vrai chiffrement, il faut un algo standard et audite.
