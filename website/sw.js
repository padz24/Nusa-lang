/* Service Worker cache-first (REQ-054). Baca offline; playground butuh lokal. */
const C='nusa-docs-0.1.0';
self.addEventListener('install',e=>{e.waitUntil(caches.open(C).then(c=>c.addAll(["./index.html", "./assets/style.css", "./assets/app.js", "./learn/", "./language/", "./search/index.json"]))});
self.addEventListener('fetch',e=>{e.respondWith(caches.match(e.request).then(r=>r||fetch(e.request)))});
