(function () {
    function ready(fn) {
        if (document.readyState === "loading") {
            document.addEventListener("DOMContentLoaded", fn);
        } else {
            fn();
        }
    }

    ready(function () {
        document.querySelectorAll("i[data-lucide]").forEach(function (icon) {
            icon.setAttribute("aria-hidden", "true");
            icon.classList.add("doc-icon");
        });

        document.querySelectorAll("pre.mermaid, .mermaid pre, pre code.language-mermaid").forEach(function (node) {
            var pre = node.tagName === "PRE" ? node : node.closest("pre") || node;
            if (!pre || pre.dataset.docDiagramLabel === "1") {
                return;
            }
            pre.dataset.docDiagramLabel = "1";
            var label = document.createElement("div");
            label.className = "doc-diagram-label";
            label.textContent = "Diagram source";
            pre.parentNode.insertBefore(label, pre);
        });

        document.querySelectorAll("a[href^='http://'], a[href^='https://']").forEach(function (link) {
            if (!link.getAttribute("target")) {
                link.setAttribute("target", "_blank");
            }
            var rel = (link.getAttribute("rel") || "").split(/\s+/).filter(Boolean);
            if (rel.indexOf("noreferrer") === -1) {
                rel.push("noreferrer");
            }
            link.setAttribute("rel", rel.join(" "));
        });
    });
})();
