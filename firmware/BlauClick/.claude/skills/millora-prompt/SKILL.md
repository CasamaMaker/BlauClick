---
name: millora-promp
description: >
  Genera prompts estructurats i d'alta qualitat per treballar en mode agent a VSCode (GitHub Copilot Agent Mode, Claude Code, Cursor, etc.).
  ACTIVACIÓ OBLIGATÒRIA: Usa aquesta skill SEMPRE que l'usuari descrigui una tasca de programació concreta, sense importar com estigui formulada.
  Exemples que han d'activar aquesta skill: "vull programar X", "que es mostri X a la web", "afegeix X a la pantalla", "que el botó faci X", "crea un endpoint per X", "que l'app faci X", "quiero programar X", "añade X", "que aparezca X", "que funcione X", "modifica X perquè faci Y", "arregla el bug de X", "refactoritza X", "crea la feature X", "implementa X".
  NO executes la tasca directament. NO miris el projecte. NO escriguis codi. PRIMER usa aquesta skill per fer les preguntes estructurades i generar el prompt per a VSCode.
---

# VSCode Agent Prompt Generator

Ajuda l'usuari a generar un prompt complet i estructurat per executar en mode agent a VSCode (Copilot Agent, Claude Code, Cursor...).

L'objectiu és que el prompt resultant sigui tan precís que l'agent pugui actuar de forma autònoma amb el mínim d'interrupcions.

> L'agent ja té accés complet al projecte des de VSCode — no cal preguntar pel stack tècnic ni pels fitxers afectats, els infereix sol.

---

## Flux de treball

### Pas 1 — Preguntes clau

Presenta totes les preguntes **en un sol missatge**. No facis les preguntes de una en una.

#### Preguntes obligatòries

| # | Camp | Descripció |
|---|------|------------|
| 1 | **Projecte** | Nom del projecte i descripció breu del seu propòsit |
| 2 | **Objectiu** | Què ha de fer exactament l'agent? Descriu el resultat final desitjat |
| 3 | **Punts concrets** | Llista de tasques o passos que l'agent ha de completar |
| 4 | **Restriccions** | Què NO ha de fer? Patrons a evitar, coses a no trencar, dependències a no tocar |

#### Preguntes opcionals (fes-les si aporta valor)

- **Patrons a seguir**: Hi ha algun fitxer o mòdul de referència d'estil o estructura?
- **Mode de revisió**: L'agent ha de demanar confirmació abans d'actes irreversibles?

---

### Pas 2 — Generar el prompt

Un cop obtingudes les respostes, genera el prompt seguint l'estructura de `references/prompt-template.md`.

**Principis de qualitat:**

- **Descomposició en subtasques**: L'agent treballa millor amb una llista ordenada de passos
- **Restriccions explícites**: El que NO ha de fer és tan important com el que ha de fer
- **Criteris d'èxit auto-generats**: Dedueix-los de l'objectiu i els punts concrets — no els preguntis a l'usuari. Han de ser verificables (build passa, test passa, comportament observable...)
- **Sense ambigüitats**: Evita paraules com "eficient" o "net" sense definir-les

---

### Pas 3 — Aprovació del prompt

Mostra el prompt generat i pregunta a l'usuari:

> "T'agrada el prompt? Vols canviar o afegir alguna cosa abans d'executar-lo?"

Espera la resposta:
- Si vol **canvis** → modifica el prompt i torna a mostrar-lo. Repeteix fins aprovació.
- Si **aprova** → continua al Pas 4.

---

### Pas 4 — Versió final per a Plan Mode

Un cop aprovat, genera la versió final adaptada per a **Plan Mode** de VSCode Copilot.

Plan Mode analitza el codebase i crea un pla d'implementació detallat **abans** de fer cap canvi. L'usuari revisa i aprova el pla, i només llavors l'agent comença a codificar.

Per activar-lo, afegeix aquest bloc al **principi** del prompt:

```
> MODE: PLAN
> Abans de fer cap canvi, analitza el codebase i genera un pla d'implementació
> pas a pas. Presenta el pla complet i espera la meva aprovació explícita
> abans d'escriure o modificar cap fitxer.
```

Mostra el prompt final complet (bloc Plan Mode + prompt aprovat) llest per copiar i enganxar.

Afegeix una nota recordant a l'usuari que seleccioni **"Plan"** al desplegable de mode del panell de Copilot Chat a VSCode.

---

## Consulta el template

Llegeix `references/prompt-template.md` per veure l'estructura exacta del prompt i exemples complets.