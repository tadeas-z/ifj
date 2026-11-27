# IFJ26

Překladač imperativního jazyka IFJ26 do mezikódu IFJcode26. Týmový projekt
předmětů IFJ a IAL na FIT VUT v Brně, 2026/2027, varianta **vv-BVS**.

## Tým a rozdělení práce

| Jméno | Login | GitHub | Balíček |
| --- | --- | --- | --- |
| Dušan Popović | xpopovd00 | [@dusan1122](https://github.com/dusan1122) | B |
| Marek Záchenský | xzachem00 | [@marekzachensky1](https://github.com/marekzachensky1) | D |
| Adam Zatloukal | xzatloa01 | [@AdamZatloukal](https://github.com/AdamZatloukal) | C |
| Tadeáš Zonyga | xzonygt00 | [@tadeas-z](https://github.com/tadeas-z) | A |

- **A, lexer (Tadeáš):** konečný automat, tokeny (`token.h`), čísla, stringy
  a víceřádkové stringy, komentáře, odsazení a logické řádky, lexikální
  chyby (kód 1), testy lexeru.
- **B, parser (Dušan):** LL parser programu a příkazů, precedenční analýza
  výrazů, syntaktické chyby (kód 2), testy parseru.
- **C, sémantika (Adam):** tabulka symbolů (vv-BVS), AST, dynamické pole,
  společné typy a chybové kódy (`comp_error.h`), sémantické kontroly
  (kódy 3–7 a 10), testy v GoogleTestu.
- **D, generování kódu (Marek):** generování IFJcode26, vestavěné funkce,
  `main`, Makefile a propojení všech částí, end-to-end testy.

Rozšíření: `FUNEXP`, `VALMEM`, `BOOLTHEN` a `CONTROL` (vyžaduje
`BOOLTHEN`). Každý upraví svou část a Marek je propojí.

## Struktura

```text
inc/                     veřejná rozhraní modulů
src/main.c               vstupní bod překladače
src/lexer/               A
src/parser/              B
src/symtable/            C
src/semantic/            C
src/common/              AST, dynamické pole, typy, pomocné funkce
tests/<modul>/           testy jednotlivých modulů
docs/documentation/      podklady k dokumentaci (diagramy automatu)
```

Pro odevzdání se všechny soubory zkopírují do jednoho adresáře bez
podadresářů (kontroluje to `is_it_ok.sh`).

## Překlad a testy

```sh
make            # vytvoří build/ifj26
make clean
./build/ifj26 < program.ifj26 > program.ifjcode26
```

Testy lexeru:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Iinc src/lexer/lexer.c tests/lexer/test_lexer.c -o test_lexer && ./test_lexer
```

Testy tabulky symbolů, AST a sémantiky používají GoogleTest (`libgtest-dev`).

Překlad musí projít s gcc 14/15/16 nebo clang 19/23 na serveru Merlin.

## Dodatečné nástroje

- [Vizualizace automatu lexeru](https://ifj26-lexer.zonyga-tadeas.workers.dev/)
  ukazuje krok po kroku, jak automat zpracovává vstup.
