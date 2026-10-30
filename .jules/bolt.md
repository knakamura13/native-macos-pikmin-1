
## 2026-10-29 - Avoid reserve(size() + n) in per-frame append loops

**Learning:** `std::vector::reserve(size() + n)` inside an append loop (such as per-draw shadow stream recording) forces exact size allocations instead of exponential growth. When combined with `.clear()` at frame end, this causes vector reallocations on almost every draw call once capacity is exceeded.
**Action:** Remove `reserve(size() + n)` from append paths in hot loops to allow standard geometric growth (`2x`) and maintain steady-state capacity across frame `clear()` calls without reallocation.
