# Bug log

Every bug that took more than ~20 minutes goes here, written up **the day it
is fixed**. You will not remember the details in week 8, and by the end this
file is worth more to a reader than a thousand lines of the runtime itself.

Format, one section per bug:

---

## BUG-001 — <one-line summary>

- **Found:** <date> — <how: TSAN / a hanging benchmark / simulation seed 0x…>
- **Symptom:** what you actually observed, before you understood it.
- **Reproduce:** the exact command. If it is a simulation bug, the seed.
- **Root cause:** what was actually wrong, in two or three sentences.
- **Fix:** what changed, and why that is sufficient rather than a band-aid.
- **Prevention:** the test, assertion, or invariant added so the whole *class*
  of bug cannot recur.
- **Time to find / time to fix:** be honest, the ratio is usually interesting.

---

(no bugs yet — this is a good sign only because there is no code yet)
