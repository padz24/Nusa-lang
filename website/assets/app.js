/* Nusa Docs app.js — vanilla, tanpa dependensi. REQ-038/041/042/052/067. */
(function () {
  "use strict";

  /* Hamburger */
  var ham = document.getElementById("hamburger");
  if (ham) {
    ham.addEventListener("click", function () {
      var n = document.querySelector("nav.global");
      if (n) n.classList.toggle("open");
    });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape") {
        var n2 = document.querySelector("nav.global");
        if (n2) n2.classList.remove("open");
      }
    });
  }

  /* Copy buttons untuk setiap pre>code (REQ-041). Tanpa JS tetap terbaca. */
  document.querySelectorAll("pre").forEach(function (pre) {
    if (pre.querySelector(".copy-btn")) return;
    var b = document.createElement("button");
    b.className = "copy-btn";
    b.type = "button";
    b.textContent = "Copy";
    b.setAttribute("aria-label", "Salin blok kode");
    b.addEventListener("click", function () {
      var t = pre.innerText.replace(/^Copy\n/, "");
      function done() { b.textContent = "OK"; setTimeout(function () { b.textContent = "Copy"; }, 1200); }
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(t).then(done, function () { fallback(); });
      } else { fallback(); }
      function fallback() {
        var r = document.createRange();
        r.selectNodeContents(pre);
        var s = getSelection();
        s.removeAllRanges(); s.addRange(r);
        try { document.execCommand("copy"); done(); } catch (e) { /* manual select */ }
      }
    });
    pre.appendChild(b);
  });

  /* Learn progress via localStorage (REQ-024), tanpa akun. */
  document.querySelectorAll("[data-progress-key]").forEach(function (box) {
    var k = "nusa-progress-" + box.getAttribute("data-progress-key");
    box.querySelectorAll('input[type="checkbox"]').forEach(function (cb, i) {
      var key = k + "-" + i;
      try { cb.checked = localStorage.getItem(key) === "1"; } catch (e) {}
      cb.addEventListener("change", function () {
        try { localStorage.setItem(key, cb.checked ? "1" : "0"); } catch (e) {}
      });
    });
  });

  /* Filter generik berbasis data-attrs (RFC status, changelog versi). */
  document.querySelectorAll("[data-filter-target]").forEach(function (sel) {
    sel.addEventListener("change", function () {
      var v = sel.value;
      var target = sel.getAttribute("data-filter-target");
      document.querySelectorAll(target).forEach(function (el) {
        var tags = (el.getAttribute("data-tags") || "").split(" ");
        el.style.display = (v === "all" || tags.indexOf(v) >= 0) ? "" : "none";
      });
    });
  });

  /* Search offline (REQ-037..039): baca search/index.json, ranking sederhana. */
  var sInput = document.getElementById("q");
  var sVer = document.getElementById("qver");
  var sOut = document.getElementById("results");
  var INDEX = null;
  function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  }
  function score(entry, q) {
    q = q.toLowerCase();
    var sc = 0;
    if ((entry.id || "").toLowerCase() === q) sc += 100;
    if ((entry.title || "").toLowerCase().indexOf(q) >= 0) sc += 80;
    if ((entry.signature || "").toLowerCase().indexOf(q) >= 0) sc += 70;
    (entry.keywords || []).forEach(function (k) { if (String(k).toLowerCase().indexOf(q) >= 0) sc += 50; });
    if ((entry.snippet || "").toLowerCase().indexOf(q) >= 0) sc += 20;
    if (entry.version && entry.version.charAt(0) === "2") sc += 15;
    if ((entry.tags || "").indexOf("historic") >= 0) sc -= 20;
    return sc;
  }
  function runSearch() {
    if (!sInput || !sOut || !INDEX) return;
    var q = sInput.value.trim();
    var vf = sVer ? sVer.value : "all";
    if (q.length < 2) { sOut.innerHTML = "<p>Ketik minimal 2 karakter.</p>"; return; }
    var res = [];
    INDEX.forEach(function (e) {
      if (vf !== "all" && e.version && e.version !== vf && vf !== "2.x" && vf !== "1.x") return;
      if (vf === "2.x" && e.version && e.version.charAt(0) !== "2") return;
      if (vf === "1.x" && e.version && e.version.charAt(0) !== "1") return;
      var s = score(e, q);
      if (s > 0) res.push([s, e]);
    });
    res.sort(function (a, b) { return b[0] - a[0]; });
    res = res.slice(0, 20);
    if (!res.length) { sOut.innerHTML = "<p>Tidak ada hasil untuk <code>" + esc(q) + "</code>.</p>"; return; }
    sOut.innerHTML = res.map(function (r) {
      var e = r[1];
      return '<div class="card"><a href="' + esc(e.url) + '"><b>' + esc(e.title) + "</b></a> " +
        '<span class="badge">' + esc(e.category || "") + "</span> " +
        '<span class="badge">' + esc(e.version || "") + "</span><br><small>" + esc(e.snippet || "").slice(0, 160) + "</small></div>";
    }).join("");
  }
  if (sInput) {
    var base = document.querySelector('meta[name="nusa-base"]');
    var prefix = base ? base.getAttribute("content") : ".";
    fetch(prefix + "/search/index.json").then(function (r) { return r.json(); }).then(function (j) {
      INDEX = j;
      var params = new URLSearchParams(location.search);
      if (params.get("q")) { sInput.value = params.get("q"); runSearch(); }
    }).catch(function () {
      if (sOut) sOut.innerHTML = "<p>Indeks offline belum termuat. Gunakan <a href='../search/alpha.html'>indeks alfabetis</a>.</p>";
    });
    sInput.addEventListener("input", runSearch);
    if (sVer) sVer.addEventListener("change", runSearch);
  }

  /* Isi contoh ke editor saat dipilih (user-triggered). */
  var pgSample = document.getElementById("pg-sample");
  if (pgSample) {
    pgSample.addEventListener("change", function () {
      var ed = document.getElementById("pg-code");
      if (ed) { ed.value = pgSample.value; ed.focus(); }
    });
  }

  /* Playground client (REQ-042/044/063): POST ke 127.0.0.1, degradasi elegan. */
  var pgRun = document.getElementById("pg-run");
  if (pgRun) {
    pgRun.addEventListener("click", function () {
      var code = document.getElementById("pg-code").value;
      var out = document.getElementById("pg-out");
      var status = document.getElementById("pg-status");
      if (code.length > 256 * 1024) { status.textContent = "Kode > 256 KB ditolak."; return; }
      if (code.indexOf("\0") >= 0) { status.textContent = "NUL byte ditolak."; return; }
      status.textContent = "Menjalankan via nusa playground lokal…";
      out.textContent = "";
      var params = new URLSearchParams(location.search);
      var port = params.get("port") || "8321";
      fetch("http://127.0.0.1:" + port + "/api/run", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code: code })
      }).then(function (r) { return r.json(); }).then(function (j) {
        status.textContent = "verdict=" + (j.verdict || "?") + " rc=" + (j.rc != null ? j.rc : "?");
        out.textContent = (j.stdout || "") + (j.stderr ? "\n[stderr]\n" + j.stderr : "");
        var share = document.getElementById("pg-share");
        if (share) share.href = location.pathname + "?code=" + encodeURIComponent(code).slice(0, 4000);
      }).catch(function () {
        status.textContent = "Playground lokal tidak aktif.";
        out.textContent = "Jalankan lokal:\n  nusa playground --port " + port + "\n  # lalu klik Run lagi\nAtau: nusa run file.ns";
      });
    });
    var pc = new URLSearchParams(location.search).get("code");
    if (pc) document.getElementById("pg-code").value = pc;
  }
})();
