# Prompt Template per a VSCode Agent Mode

Usa aquesta estructura per generar el prompt al Pas 2.
Les seccions marcades com (opcional) s'inclouen només si l'usuari ho ha indicat.

> No incloguis Stack tècnic ni Fitxers afectats — l'agent els infereix del projecte.

---

## Template del prompt (Pas 2)

```markdown
## Projecte
[Nom del projecte]: [Descripció breu del seu propòsit]

## Objectiu
[Descripció clara i concisa del resultat final. Una o dues frases màxim.]

## Tasques a realitzar
1. [Primer pas concret i accionable]
2. [Segon pas]
3. [...]

## Patrons a seguir (opcional)
- Segueix l'estructura de `[fitxer de referència]`
- Mantén la convenció de noms [descripció]

## Restriccions
- NO modificar [component o comportament crític]
- NO afegir noves dependències sense confirmació
- [Altres restriccions específiques]

## Criteris d'èxit
- [ ] [Comportament verificable 1 — deduït de l'objectiu]
- [ ] [Comportament verificable 2 — deduït dels punts concrets]
- [ ] El build/compilació passa sense errors

## Instruccions per a l'agent (opcional)
- Demana confirmació abans de [acció irreversible concreta]
```

---

## Versió final per a Plan Mode (Pas 4)

Quan l'usuari aprovi el prompt, presenta la versió final amb el bloc de Plan Mode al principi:

```markdown
> MODE: PLAN
> Abans de fer cap canvi, analitza el codebase i genera un pla d'implementació
> pas a pas. Presenta el pla complet i espera la meva aprovació explícita
> abans d'escriure o modificar cap fitxer.

---

## Projecte
[... contingut del prompt aprovat ...]
```

Segueix la nota:
> 💡 Selecciona **"Plan"** al desplegable de mode del panell Copilot Chat a VSCode abans d'enganxar aquest prompt.

---

## Com generar els Criteris d'èxit automàticament

| Tipus de tasca | Criteris típics |
|---|---|
| Nova feature | Funcionalitat accessible, tests passen, build OK |
| Bugfix | El bug ja no es reprodueix, no hi ha regressions, build OK |
| Refactoring | Comportament extern idèntic, tests existents passen |
| Nova API/endpoint | Endpoint retorna el codi HTTP esperat, validació funciona |
| Migració de BD | Migració s'aplica sense errors, dades existents intactes |

---

## Exemples complets

### Exemple 1 — Nova feature

**Prompt generat (Pas 2):**
```markdown
## Projecte
shop-api: API REST per a una botiga en línia amb gestió d'inventari.

## Objectiu
Crear un endpoint POST /api/orders que permeti crear una comanda,
descomptant l'estoc dels productes i enviant una notificació per email.

## Tasques a realitzar
1. Crear el model `Order` i `OrderItem` a l'schema de Prisma
2. Executar la migració
3. Crear `OrderService` seguint el patró dels altres services existents
4. Crear el controlador i registrar la ruta
5. Afegir tests unitaris

## Restriccions
- NO modificar el model `Product` existent
- NO canviar l'autenticació JWT actual
- El descompte d'estoc ha de ser transaccional

## Criteris d'èxit
- [ ] POST /api/orders retorna 201 amb la comanda creada
- [ ] L'estoc dels productes es descompta correctament
- [ ] Tests passen: `npm test`
- [ ] Build sense errors: `npm run build`
```

**Versió final per a Plan Mode (Pas 4):**
```markdown
> MODE: PLAN
> Abans de fer cap canvi, analitza el codebase i genera un pla d'implementació
> pas a pas. Presenta el pla complet i espera la meva aprovació explícita
> abans d'escriure o modificar cap fitxer.

---

## Projecte
shop-api: API REST per a una botiga en línia amb gestió d'inventari.

## Objectiu
Crear un endpoint POST /api/orders que permeti crear una comanda,
descomptant l'estoc dels productes i enviant una notificació per email.

## Tasques a realitzar
1. Crear el model `Order` i `OrderItem` a l'schema de Prisma
2. Executar la migració
3. Crear `OrderService` seguint el patró dels altres services existents
4. Crear el controlador i registrar la ruta
5. Afegir tests unitaris

## Restriccions
- NO modificar el model `Product` existent
- NO canviar l'autenticació JWT actual
- El descompte d'estoc ha de ser transaccional

## Criteris d'èxit
- [ ] POST /api/orders retorna 201 amb la comanda creada
- [ ] L'estoc dels productes es descompta correctament
- [ ] Tests passen: `npm test`
- [ ] Build sense errors: `npm run build`
```