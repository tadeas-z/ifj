# Lokální překlad:  latexmk        (výstup dokumentace.pdf v kořeni)
#                   latexmk -c     (smaže pomocné soubory)
# Pomocné soubory jdou do build/, ať se nepletou mezi zdrojáky.
use Cwd;
$pdf_mode = 1;
$aux_dir  = 'build';
@default_files = ('dokumentace.tex');
# BibTeX běží v build/, literatura.bib je v kořeni
ensure_path('BIBINPUTS', getcwd());
# MiKTeX (Windows) BIBINPUTS ignoruje -> předat adresář přímo
if ($^O eq 'MSWin32' || $^O eq 'msys') {
  $bibtex = 'bibtex --include-directory="' . getcwd() . '" %O %S';
}
