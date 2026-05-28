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

> L'agent ja té accés complet al projecte des de VSCode — **no cal preguntar pel projecte ni pel stack tècnic**, els infereix del context.
> La restricció és sempre la mateixa: **només modificar el que l'usuari indica, res més**.

---

## Flux de treball

### Pas 1 — Pregunta l'objectiu

Fes **només** aquesta pregunta, i espera la resposta:

> "Quin és l'objectiu? Descriu el resultat final que vols aconseguir."

---

### Pas 2 — Pregunta els punts concrets

Un cop rebuda la resposta anterior, fes **només** aquesta pregunta, i espera la resposta:

> "Quins són els punts concrets? Llista les tasques o passos que l'agent ha de completar."

---

### Pas 3 — Generar el prompt

Un cop obtingudes les dues respostes, genera el prompt **directament des d'aquí**, sense llegir cap fitxer. Com a molt, pots llegir línies seleccionades de `references/prompt-template.md` si necessites un exemple concret, però no és necessari.

**Estructura del prompt:**

```markdown
## Objectiu
[Resultat final clar i concís. Una o dues frases.]

## Tasques a realitzar
1. [Pas concret i accionable]
2. [...]

## Restriccions
- NO modificar res que no s'hagi especificat explícitament. Només tocar el que s'indica.

## Criteris d'èxit
- [ ] [Comportament verificable deduït de l'objectiu]
- [ ] [Comportament verificable deduït dels punts concrets]
- [ ] El build/compilació passa sense errors
```

**Regles fixes (no preguntar a l'usuari):**
- **Projecte**: inferit del context de la conversa — no s'inclou al prompt
- **Restriccions**: sempre la mateixa restricció hardcoded de dalt
- **Criteris d'èxit**: deduïts automàticament — verificables (build passa, test passa, comportament observable...)

**Principis de qualitat:**
- **Descomposició en subtasques**: llista ordenada de passos accionables
- **Sense ambigüitats**: evita paraules com "eficient" o "net" sense definir-les

Mostra el prompt generat i pregunta:

> "T'agrada el prompt? Vols canviar o afegir alguna cosa abans d'executar-lo?"

Espera la resposta:
- Si vol **canvis** → modifica el prompt i torna a mostrar-lo. Repeteix fins aprovació.
- Si **aprova** → continua al Pas 4.

---

### Pas 4 — Executar en Plan Mode

Un cop aprovat, genera i **executa directament** la versió final adaptada per a **Plan Mode**.

Afegeix aquest bloc al **principi** del prompt:

```
> MODE: PLAN
> Abans de fer cap canvi, analitza el codebase i genera un pla d'implementació
> pas a pas. Presenta el pla complet i espera la meva aprovació explícita
> abans d'escriure o modificar cap fitxer.
```

Mostra el prompt final complet (bloc Plan Mode + prompt aprovat) llest per copiar i enganxar.

Afegeix una nota recordant a l'usuari que seleccioni **"Plan"** al desplegable de mode del panell de Copilot Chat a VSCode.