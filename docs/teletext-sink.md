# Teletext Sink

| | |
|-|-|
| **Stage id** | `teletext_sink` |
| **Stage name** | Teletext Sink |
| **Connections** | 1 input → no outputs |
| **Purpose** | Recover World System Teletext from the VBI, export the packet stream, and browse the pages the recording carried |

**Use this stage when:**

* Preserving teletext carried by a LaserDisc, CVBS capture, or tape source
* Reading the pages a recording carried without leaving decode-orc
* Producing a packet stream for external teletext tools (vhs-teletext, wxTED)

**What it does**

Triggering the stage makes one linear pass over the whole frame range. It probes the candidate VBI lines of both fields of every frame for teletext data lines, recovers the packets, and writes them as a flat, headerless packet stream in strictly temporal order (frame → field → ascending line). Frames are decoded on several threads while the stream is written from one, and the pass learns where in the line this particular recording puts its data and which lines carry it, so later frames cost far less than the first ones — together these take a pass over the reference captures from 1970 ms to 295 ms (625-line) and 946 ms to 156 ms (525-line). None of it changes what is recovered: see `decode_threads`, `pin_data_phase` and `learn_active_lines` below. Packets keep their transmission coding (Hamming 8/4 addressing, odd-parity display bytes), so consumers decode the stream exactly as a receiver decodes a live broadcast.

Both television systems ITU-R BT.653 defines System B on are covered, and the service decides the file the run writes:

* **625 lines** (PAL) — ETSI EN 300 706, 42-byte packets, written as `.t42`
* **525 lines** (NTSC, PAL-M) — BT.653 Table 1b, 34-byte packets, written as `.t34`

The same pass assembles the pages, so the run also produces a catalogue of every page the recording carried — where each was first and last seen, how often the carousel brought it round, and its best assembly from every copy recovered. That is what the **Teletext Pages** tool shows. Because it comes from a pass over the whole source rather than a window around the preview position, the list is the service's full carousel. A page number transmitted as a sequence of sub-pages (ETSI EN 300 706 Annex A.1 — on a receiver, a page that changes on its own every few seconds) is catalogued as the whole sequence, one assembly per sub-page.

Recovery quality tracks the source's luma bandwidth: LaserDisc and broadcast-quality CVBS captures are read exactly by threshold slicing, while consumer VHS loses the clock run-in entirely and needs the MLSE detector, which recovers readable pages from PAL SP and LP recordings. The default `detector` setting picks between the two per line, so neither source needs configuring. By default the stage also mends display bytes that fail their parity check and combines the repeated transmissions of each page row before writing (`repair_damaged_bytes`, `squash_repeated_rows`); turn both off to write the packets exactly as recovered.

**Parameters**

* `output_path` (file path)
    - Path to the output packet stream. The service's extension is appended if absent — `.t42` on a 625-line source, `.t34` on a 525-line one.
    - Optional. Left empty, the run decodes exactly as it would but writes no file, which is what to do when the pages themselves are what you are after — the **Teletext Pages** tool is filled either way. `write_report`, `export_data` and `export_subtitles` are written beside the packet stream, so enabling any of them without a path fails the run.
* `first_vbi_line` (integer)
    - First candidate field line probed, 1-based, both fields. Range: 1–22.
    - Default: `6` on a 625-line project, `10` on a 525-line one. The 625-line window 6–22 covers broadcast lines 6–22 and 318–335 (ETSI EN 300 706); the 525-line window 10–21 covers lines 10–21 and 273–284 (ITU-R BT.653 §2).
* `last_vbi_line` (integer)
    - Last candidate field line probed, 1-based, both fields. Range: 1–22.
    - Default: `22` on a 625-line project, `21` on a 525-line one.
* `keep_empty_packets` (boolean)
    - Emit a whole zero packet for candidate lines with no data so packet position maps 1:1 to (frame, field, line) — the vhs-decode convention.
    - Default: `false`.
* `detector` (string)
    - How data bits are recovered: `Threshold` (slice at bit centres; exact on discs and direct captures), `MLSE` (fit the recording's frequency response to the known start of each line, detect against it, then refit that response to the whole packet just read and read it again; recovers teletext from tape, where limited bandwidth smears bits into their neighbours), or `Automatic` (threshold first, MLSE only where it fails — same behaviour and cost as threshold alone on a disc source).
    - Default: `Automatic`.
* `character_set` (string)
    - Which alphabet the page codes are read in: `Latin`, `Cyrillic (Russian/Bulgarian)`, `Cyrillic (Ukrainian)` or `Cyrillic (Serbian/Croatian)`. A display code means nothing on its own — 4/4 is `D` in the Latin set and `Д` in the Russian one — and a service that says which set it uses (a packet X/28/0 or M/29/0, ETSI EN 300 706 §15.2) is always believed, so this is consulted only for pages that designate nothing. That is most of what survives on tape: those packets are a Level 2.5 facility, and for Level 1 the standard falls back on "a local Code of Practice" — the region the receiver was sold in. Nothing in the recording can settle it, so the choice has to be yours. It affects only the pages you read and any subtitles exported from them; the exported packet stream is the transmitted bytes and carries no character set. Within Latin, the national option sub-set (`£` for a UK service, `é` for a French one) still comes from each page's own header bits and needs no setting. Greek, Arabic and Hebrew G0 sets are not implemented; a page designating one is shown as Latin. Changing the setting needs a fresh trigger, since the pages are decoded during the run; an already exported packet stream does not need exporting again.
    - Default: `Latin`.
* `second_character_set` (string)
    - The alphabet the `ESC` control character switches into, for services that mix two on one page — a Russian or Ukrainian service writes foreign names in Latin letters in among the Cyrillic, marking each switch with the code that toggles the rest of the display row into the other alphabet (ETSI EN 300 706 §12.2 Table 26 code 1/B, §15.3). `None` or one of the four sets `character_set` offers. As with `character_set`, a service that designates its own pair is always believed and this is ignored — including when it says it uses only one, which switches the control code off. `None` means the same for material that designates nothing: one alphabet throughout and the code inert, which is how pages were read before. Set it only where a recording really does mix alphabets; a wrong pairing turns the switched runs into nonsense rather than leaving them alone. For a Russian service the pair is `character_set` = `Cyrillic (Russian/Bulgarian)` with `second_character_set` = `Latin`. The pair is re-selected at the start of every row, so a lost `ESC` corrupts at most the rest of one row; a second Latin set always uses the English national sub-set (§15.3).
    - Default: `None`.
* `tolerant_framing` (boolean)
    - Accept framing codes with one bit error (more packets from noisy sources, higher false-positive rate).
    - Default: `false`.
* `require_valid_mrag` (boolean)
    - Drop packets whose magazine/row address fails Hamming 8/4 correction (suppresses false locks on noise).
    - Default: `true`.
* `repair_damaged_bytes` (boolean)
    - Every display byte carries a parity bit, so a byte that fails its parity check is known to be damaged. Restore it by flipping the bit the MLSE detector came closest to reading the other way. Recovers characters a difficult tape would otherwise lose; the cost is that a repaired byte can no longer be told from an undamaged one, so a repair that guessed wrong is no longer marked as damage. Applies to the MLSE detector only, so a disc or direct capture is unaffected; offered only when `detector` is `Automatic` or `MLSE`.
    - Default: `true`.
* `pin_data_phase` (boolean)
    - Most of the work of reading a line is searching the whole of the standard's data-timing window for where the data burst starts. Every line of a time-base-corrected recording starts at very nearly the same place, so once enough lines have been read the search narrows to where they agreed. A narrowed search that finds nothing is repeated over the full window, so this cannot lose a packet; it costs a few percent on lines that carry no data.
    - Measured on the reference captures this is the larger of the two savings, and it also *recovers* packets an exhaustive search misses — narrowing the window rejects the false correlation peaks a whole-window search can settle on. The report says where the window was pinned, or why it was not.
    - Default: `true`.
* `learn_active_lines` (boolean)
    - A service uses a few of the lines its standard permits, but every line of the window is read on every frame, and on a line carrying picture content, VITS, VITC or captions that work is spent reaching a rejection already reached on the frame before. Read every line for the first 50 frames, then only the lines that have carried a packet, rechecking the full window every 50th frame so a service that starts part way into a recording is still picked up.
    - Unlike `pin_data_phase` this can lose a packet: a line that carries data exactly once, outside both the learning frames and a recheck frame, is not read. On the reference PAL capture that cost one packet in 3,964. Turn it off for an archival pass where every packet matters.
    - Default: `true`.
* `decode_threads` (integer)
    - Threads to recover lines on; `0`, the default, uses one per processor. Range: 0–256. Each line is recovered from its own samples, so frames are decoded several at a time while the packets are written from one thread in the order they were transmitted.
    - The recovered stream is identical whatever this is set to, so lower it only to leave the machine free for other work. Measured on the 625-line reference capture: 1406 ms on one thread, 442 on four, 295 on eight; past the processor's physical cores there is nothing more to win, because the run is by then waiting on the source stage to hand out frames.
    - Default: `0`.
* `squash_repeated_rows` (boolean)
    - Teletext pages are transmitted on a loop, so a recording holds several copies of every page row, damaged in different places. Combine them byte by byte — preferring values that pass their parity check, then weighting by how sure the detector was of each byte — and write the combined rows. Packet order, count and timing are unchanged; only damaged display bytes move. The pages shown in the viewer are built from the combined rows too. Needs a second pass over the recovered packets, which are kept in a scratch file on disk between the passes (about 50 bytes each, so 1–2 GB for a six-hour tape) in the per-user cache directory, or under `ORC_CACHE_DIR` if that is set.
    - Copies are combined only within one run of one sub-page: a header with the erase bit set (C4) says the content is being replaced, so what follows is a different page sharing a number. A service that erases on every transmission gives each one a run of its own, and nothing can be combined — the report says so, as a run count matching the transmission count.
    - Packets of other pages are kept out of the vote. A row packet is attributed to a page only when its address arrived as a valid codeword rather than being corrected into one (a corrected one is still written to the stream, but no page votes it in), and the vote is taken twice, the second time without the copies that agree with less than half of the first.
    - Default: `true`.
* `write_report` (boolean)
    - Write the run's diagnostic report next to the packet stream under its full name plus `.txt` (`mydata.t42` gives `mydata.t42.txt`, `mydata.t34` gives `mydata.t34.txt`), so it needs `output_path` set. It opens with the result in one line — `Data loss 1.14% — 30 of 2,640 recovered characters are damaged` — and the same figure appears in the stage's status when the run finishes. Below that it covers what was exported, how recovery went, how many pages were catalogued, and what combining repeated rows changed. The same report always goes to the log at debug level.
    - Damage is counted by the odd parity every display byte carries, over the display rows as written. It is a floor rather than an exact count — a byte damaged in two bits passes parity — and it says nothing about rows that never arrived.
    - Default: `false`.
* `export_data` (boolean)
    - Write everything the run decoded into a folder beside the packet stream, named after it plus `.export` (`mydata.t42.export`): the pages as text with every version kept, the broadcast service data, the data lines' payloads, the report and a README. The same files the **Teletext Pages** viewer's **Export All…** writes under `data/`, and the way to get them from `orc-cli`.
    - Default: `false`.
* `export_subtitles` (boolean)
    - Decode the subtitle page alongside the packet export and write timed cues to a `.srt` file next to the output. Offered on 625-line projects only: the cue timing derives from 50 fields per second. Text is written as UTF-8 through the page's own character set, so a UK page's `£` and a Cyrillic service's cues both reach the file as themselves.
    - Default: `false`.
* `subtitle_page` (string)
    - Teletext page carrying the subtitles: magazine digit (1–8) plus two hexadecimal page digits, e.g. `888`. Offered when `export_subtitles` is on.
    - Default: `888`.
* `subtitle_format` (string)
    - Subtitle output format; currently `SRT` (SubRip) only. Offered when `export_subtitles` is on.
    - Default: `SRT`.

**Stage tools**

* **Teletext Pages** — the page viewer for this node. It lists every page the range carried, with how many times each was seen and the frames it was first and last seen at, and renders the selected page as a Level 1 display alongside the run's recovery summary. It opens automatically when the node is triggered, which is how the pages are reached: leave `output_path` empty and triggering the node is a decode-and-browse with no file written. The catalogue stays with the stage afterwards, so closing the viewer and picking **Teletext Pages** from the **Stage Tools** menu re-opens it immediately rather than decoding the source again; on a node that has not been triggered that entry says there is nothing to show instead of starting the decode itself. Editing any stage's parameters rebuilds the graph and discards the catalogue with it, closing the viewer — trigger again to rebuild it from the new settings. **Save PNG…** writes the page on screen out as an image, drawn on its own at whole pixels per character rectangle — a 40 x 25 page comes out 960 x 1000 — with flashing characters held lit and the data-error overlay included if **Show data errors** is switched on. **Save All PNGs…** writes every page and sub-page in the catalogue into a chosen folder in one pass, each the image **Save PNG…** would have written for it and named the same way — `Page-100-0001.png`, `Page-100-0002.png`, and so on — with a progress dialogue that can be cancelled partway.
    - Where a page number carries a sequence of sub-pages, the control under the display says how many there are, which one is on screen and its sub-code (`0001`, `0002`, …), and the arrows either side step through the sequence, wrapping round at each end. The status line then describes the sub-page on screen rather than the whole set; the page list keeps counting the set together, because a row there is about the page number. A page's row says in its tooltip when it is a sequence.
    - Characters are drawn in the typeface of the receiver's own character generator, the Mullard SAA5050 series — the chip that put a page on screen on a British receiver and on the BBC Micro in Mode 7 — including the character rounding it applied on the way to the display, which is what gives teletext letterforms their sloped diagonals. Both the Latin and the Cyrillic G0 sets are covered, the latter from the SAA5057 that drew them. A character the family never held is drawn in the platform's monospaced font instead, which looks out of place but stays legible.
    - **Enable animations** flashes the characters the service marked to flash, on the 0.75 Hz cycle a World System Teletext receiver used — three quarters of each second showing the character and the last quarter blank. Only the characters alternate; their background stays put. Clear the box to hold the page still, which is what reading a flashing headline or capturing the display wants.

**Notes**

* PAL, NTSC and PAL-M sources are accepted; any other video system reports an error. NABTS (System C) shares the 525 lines but not the framing code, so its lines are seen and rejected rather than decoded — use the **NABTS Sink** for those. French System A and Japanese System D are not supported, and NTSC line-21 captions are handled by the Closed Caption Sink instead.
* **Phantom page numbers are pruned.** A three-bit burst can resolve a Hamming 8/4 page-number digit silently into a neighbouring codeword (MLSE damage commonly carries digit 0 onto 7), cataloguing a page at a number the service never sent. A page number no appearance ever named as transmitted is folded into the one attested number a single hexadecimal digit away, or removed if there are several such or none; the report says how many were folded and dropped. On an undamaged source nothing changes.
* This stage writes no CSV — its file output is the packet stream, and optionally the subtitle document and the report.
* The `.t42` format is described on the zxnet teletext wiki (T42 packet stream); `.t34` is the same flat, headerless convention at the 525-line packet length.
* A 525-line service sends the last eight columns of its rows in separate row-extension packets, which the page viewer reassembles; the packet stream holds them as transmitted.
* Subtitle export drops Level 1 colour and positioning attributes; the `.srt` carries plain text timed from the field rate. With `squash_repeated_rows` enabled the cues are decoded from the combined rows, so they benefit from the same correction.
* Combining repeated rows ("squashing") is an idea taken from [vhs-teletext](https://github.com/ali1234/vhs-teletext) by Alistair Buxton. A row transmitted only once cannot be corrected, so the benefit grows with how long the recording runs and how often each page recurs.

---

