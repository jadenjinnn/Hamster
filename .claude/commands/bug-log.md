---
description: Log a bug. Fast capture, minimal interruption.
---

I want to log a bug. Goal: capture quickly, return me to whatever I was doing.

## Step 1: Quick capture

Ask me ONLY these (one message, no follow-ups):

1. **Symptom** — what's wrong, observably?
2. **Severity guess** — Critical / High / Medium / Low (one word)
3. **Suspected location** — file or area if I have a guess, "unknown" if not
4. **Reproduction** — quick steps if I have them, "needs investigation" if not

If I'm clearly mid-task and skipped any field, accept what I gave. Don't loop.

## Step 2: Create the bug file

1. Determine the next bug ID: scan `docs/bugs/active/` and `docs/bugs/closed/` for the highest existing 4-digit ID, increment by 1.
2. Pick a kebab-case slug from the symptom (3-5 words max).
3. Create `docs/bugs/active/<id>-<slug>.md` from `.claude/templates/bug.md`.
4. Fill in: title, status=open, severity from my answer, logged date (today), found-while (the task I'm currently on, infer from session context).
5. Fill in symptom, suspected location, reproduction from my answers.
6. Leave investigation, root cause, fix, verification empty — they get filled at fix time.
7. Tier stays blank.

## Step 3: TODO comment (optional)

If I gave a specific suspected file/line, ask me one yes/no: "Add `// TODO(bug:NNNN): <symptom>` at <location>?"

If yes, add it. If no or I don't answer, skip.

## Step 4: Hand back

Tell me the bug ID and confirm I can resume. One sentence:

> Logged as bug NNNN. Resuming <whatever I was doing>.

Do not start investigating. Do not propose fixes. Do not switch context. The whole point of this command is *not* to break my flow.
