// ============================================
// MOBILE MENU
// ============================================
document.getElementById("mobile-menu-btn").addEventListener("click", function () {
	document.getElementById("mobile-menu").classList.toggle("active");
});

// Close mobile menu when clicking a link
document.querySelectorAll("#mobile-menu a").forEach((link) => {
	link.addEventListener("click", function () {
		document.getElementById("mobile-menu").classList.remove("active");
	});
});
// ============================================
// FILE UPLOAD PREVIEW (POST) - UPDATED
// ============================================
const fileInput = document.getElementById("fileInput");
const fileInputText = document.getElementById("fileInputText");
const fileInputLabel = document.querySelector(".file-input-label");
const preview = document.getElementById("preview");
const fileInfo = document.getElementById("fileInfo");

fileInput.addEventListener("change", function (e) {
	const file = e.target.files[0];

	if (!file) {
		// Reset to default state
		fileInputText.textContent = "Choose a file...";
		fileInputLabel.classList.remove("has-file");
		preview.innerHTML = '<span id="previewPlaceholder" class="text-text-secondary">No file selected</span>';
		fileInfo.textContent = "";
		return;
	}

	// Update file input label with filename
	fileInputText.textContent = file.name;
	fileInputLabel.classList.add("has-file");

	// Update file info
	fileInfo.textContent = `${file.name} (${(file.size / 1024).toFixed(2)} KB)`;

	// Preview logic
	if (file.type.startsWith("image/")) {
		const reader = new FileReader();
		reader.onload = function (e) {
			preview.innerHTML = `<img src="${e.target.result}" class="preview-img" alt="Preview">`;
		};
		reader.readAsDataURL(file);
	} else if (
		file.type.startsWith("text/") ||
		file.name.endsWith(".txt") ||
		file.name.endsWith(".json") ||
		file.name.endsWith(".html")
	) {
		const reader = new FileReader();
		reader.onload = function (e) {
			preview.innerHTML = `<pre class="preview-txt">${e.target.result}</pre>`;
		};
		reader.readAsText(file);
	} else {
		preview.innerHTML = `<span class="text-text-secondary text-center">
			<span class="text-4xl block mb-2">📄</span>
			<span class="text-sm">${file.name}</span>
		</span>`;
	}
});

// Reset file input after successful upload (dentro de la función showUploadMessage)
// Actualiza la parte del código que limpia el formulario:
uploadForm.addEventListener("submit", async function (e) {
	e.preventDefault();

	const formData = new FormData(uploadForm);
	const file = fileInput.files[0];

	if (!file) {
		showUploadMessage("⚠️ Please select a file to upload", "error");
		return;
	}

	uploadBtn.disabled = true;
	uploadBtnText.textContent = "⏳ Uploading...";
	uploadResult.classList.add("hidden");

	try {
		const response = await fetch("/upload", {
			method: "POST",
			body: formData,
		});

		if (response.ok) {
			showUploadMessage(`✅ File "${file.name}" uploaded successfully!`, "success");

			// Limpiar formulario después de 10 segundos
			setTimeout(() => {
				uploadForm.reset();
				fileInputText.textContent = "Choose a file...";
				fileInputLabel.classList.remove("has-file");
				preview.innerHTML = '<span id="previewPlaceholder" class="text-text-secondary">No file selected</span>';
				fileInfo.textContent = "";
			}, 10000);
		} else {
			showUploadMessage(`❌ Upload failed: ${response.status} ${response.statusText}`, "error");
		}
	} catch (error) {
		showUploadMessage(`❌ Network error: ${error.message}`, "error");
	} finally {
		uploadBtn.disabled = false;
		uploadBtnText.textContent = "📤 Upload File";
	}
});

function showUploadMessage(message, type) {
	uploadResult.textContent = message;
	uploadResult.className = type;
	uploadResult.classList.remove("hidden");

	setTimeout(() => {
		uploadResult.classList.add("hidden");
	}, 5000);
}

// ============================================
// FILE DELETION (DELETE)
// ============================================
// --- DELETE flow: populate select, preview, delete ---
// Carga la lista (GET /upload), llena el select, muestra preview y borra (DELETE /upload/:name).
// DOM elements
const refreshBtn = document.getElementById("refreshFilesBtn");
const deleteSelect = document.getElementById("deleteFileSelect");
const deletePreview = document.getElementById("deletePreview");
const deletePreviewPlaceholder = document.getElementById("deletePreviewPlaceholder");
const deleteFileInfo = document.getElementById("deleteFileInfo");
const deleteSelectedBtn = document.getElementById("deleteSelectedBtn");
const deleteResult = document.getElementById("deleteResult");

// preview base (ajusta si tus archivos se sirven desde otra ruta)
const previewUrlBase = "/upload"; // según tu log parece /upload es la ruta pública

// Utilities
function formatFileSize(bytes) {
	if (!Number.isFinite(bytes)) return "";
	if (bytes < 1024) return bytes + " B";
	if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + " KB";
	return (bytes / (1024 * 1024)).toFixed(2) + " MB";
}
function escapeHtml(str) {
	if (!str) return "";
	return String(str).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// Attempt to parse a directory listing HTML and extract filenames
function parseHtmlFileList(htmlText, basePath = "/upload") {
	try {
		const doc = new DOMParser().parseFromString(htmlText, "text/html");
		const anchors = Array.from(doc.querySelectorAll("a"));
		const files = [];
		anchors.forEach(a => {
			const href = a.getAttribute("href") || "";
			// ignore parent refs and anchors
			if (href === "../" || href === "#" || href === "/") return;
			// normalize href: if absolute or starts with basePath or is relative
			let name = href;
			// If href contains full path, extract last segment
			try {
				const url = new URL(href, window.location.origin + basePath + "/");
				name = decodeURIComponent(url.pathname.split("/").filter(Boolean).pop() || "");
			} catch (e) {
				// fallback: split by slash
				const parts = href.split("/").filter(Boolean);
				name = decodeURIComponent(parts.pop() || href);
			}
			// Basic filter: ignore entries that look like directories (ending with /) or empty names
			if (!name) return;
			if (name === "." || name === "..") return;
			// push minimal info (size/modified unknown)
			if (!files.find(f => f.name === name)) {
				files.push({ name, size: 0, modified: null });
			}
		});
		return files;
	} catch (e) {
		console.error("parseHtmlFileList error:", e);
		return [];
	}
}

// Load file list for delete select - JSON first, then HTML fallback
async function loadFileListForDelete() {
	try {
		deleteResult.classList.add("hidden");
		deleteSelect.innerHTML = '<option value="">-- Loading files... --</option>';
		deletePreview.innerHTML = '<span class="text-text-secondary">Loading preview...</span>';
		deleteSelectedBtn.disabled = true;
		deleteFileInfo.textContent = "";

		const res = await fetch("/upload", { headers: { Accept: "application/json, text/html, */*" } });
		const contentType = (res.headers.get("content-type") || "").toLowerCase();
		const text = await res.text();

		if (!res.ok) {
			// If server returned error status, show snippet for debugging
			const snippet = text ? text.slice(0, 800) : "";
			deleteSelect.innerHTML = '<option value="">-- Error loading files --</option>';
			deletePreview.innerHTML = `<div class="text-warning p-2">Error ${res.status}: ${res.statusText}<pre style="max-height:120px;overflow:auto;">${escapeHtml(snippet)}</pre></div>`;
			console.error("GET /upload returned error:", res.status, res.statusText, snippet);
			return;
		}

		let files = null;

		// If response is JSON (or content-type includes json), try parse
		if (contentType.includes("application/json")) {
			try {
				files = JSON.parse(text);
			} catch (err) {
				console.warn("Content-Type says JSON but parse failed:", err);
			}
		}

		// If not JSON, attempt to parse as JSON anyway (in case server mis-set content-type)
		if (files === null) {
			try {
				files = JSON.parse(text);
			} catch (err) {
				// JSON parse failed -> fallback to HTML parsing
				console.info("Falling back to HTML parsing of /upload response (not JSON).");
				files = parseHtmlFileList(text, "/upload");
			}
		}

		// Validate files array
		if (!Array.isArray(files) || files.length === 0) {
			deleteSelect.innerHTML = '<option value="">-- No files uploaded yet --</option>';
			deletePreview.innerHTML = '<span class="text-text-secondary">No file selected</span>';
			return;
		}

		// Fill select
		deleteSelect.innerHTML = '<option value="">-- Select a file --</option>';
		files.sort((a, b) => (a.name || "").localeCompare(b.name || ""));
		files.forEach(f => {
			const opt = document.createElement("option");
			opt.value = f.name;
			// show size if we have it, else omit
			opt.textContent = f.size ? `${f.name} (${formatFileSize(Number(f.size) || 0)})` : f.name;
			opt.dataset.size = f.size || 0;
			opt.dataset.modified = f.modified || "";
			deleteSelect.appendChild(opt);
		});

		deletePreview.innerHTML = '<span class="text-text-secondary">No file selected</span>';
	} catch (err) {
		deleteSelect.innerHTML = '<option value="">-- Error loading files --</option>';
		deletePreview.innerHTML = `<div class="text-warning p-2">Error: ${escapeHtml(err.message)}</div>`;
		console.error("loadFileListForDelete error:", err);
	}
}

// preview on select change (same as before, uses previewUrlBase)
deleteSelect?.addEventListener("change", async function () {
	const filename = this.value;
	deleteResult.classList.add("hidden");
	if (!filename) {
		deletePreview.innerHTML = '<span class="text-text-secondary">No file selected</span>';
		deleteFileInfo.textContent = "";
		deleteSelectedBtn.disabled = true;
		return;
	}

	const selOpt = this.options[this.selectedIndex];
	deleteFileInfo.textContent = `${selOpt.textContent}${selOpt.dataset.modified ? ' • ' + new Date(selOpt.dataset.modified).toLocaleString() : ''}`;

	const url = `${previewUrlBase}/${encodeURIComponent(filename)}`;
	const ext = filename.split(".").pop().toLowerCase();
	const imageExts = ["jpg", "jpeg", "png", "gif", "webp", "svg"];
	const textExts = ["txt", "json", "html", "md", "log"];

	if (imageExts.includes(ext)) {
		deletePreview.innerHTML = `<img src="${url}" alt="${escapeHtml(filename)}" class="preview-img" />`;
	} else if (textExts.includes(ext)) {
		deletePreview.innerHTML = '<div class="text-text-secondary p-2">⏳ Loading preview...</div>';
		try {
			const r = await fetch(url);
			if (!r.ok) throw new Error(`Preview fetch failed ${r.status}`);
			const txt = await r.text();
			deletePreview.innerHTML = `<pre class="preview-txt">${escapeHtml(txt.slice(0, 5000))}</pre>`;
		} catch (err) {
			deletePreview.innerHTML = `<div class="text-warning p-2">Could not load preview: ${escapeHtml(err.message)}</div>`;
		}
	} else {
		deletePreview.innerHTML = `<div class="text-text-secondary text-center"><span class="text-4xl block mb-2">📄</span><div><a href="${url}" target="_blank" class="text-accent font-semibold">Open file</a></div></div>`;
	}

	deleteSelectedBtn.disabled = false;
});

// delete selected
deleteSelectedBtn?.addEventListener("click", async function () {
	const filename = deleteSelect.value;
	if (!filename) return;
	if (!confirm(`Are you sure you want to delete "${filename}"?`)) return;

	const prevText = deleteSelectedBtn.textContent;
	deleteSelectedBtn.disabled = true;
	deleteSelectedBtn.textContent = "⏳ Deleting...";

	try {
		const res = await fetch(`/upload/${encodeURIComponent(filename)}`, { method: "DELETE" });
		const text = await res.text();

		if (!res.ok) {
			throw new Error(`Server: ${res.status} ${res.statusText}\n${text.slice(0, 500)}`);
		}

		deleteResult.textContent = `✓ File "${filename}" deleted successfully`;
		deleteResult.className = "success";
		deleteResult.classList.remove("hidden");

		// remove option, reset preview
		const optToRemove = Array.from(deleteSelect.options).find(o => o.value === filename);
		if (optToRemove) optToRemove.remove();
		deleteSelect.value = "";
		deletePreview.innerHTML = '<span class="text-text-secondary">No file selected</span>';
		deleteFileInfo.textContent = "";
		deleteSelectedBtn.disabled = true;

		const hasFiles = Array.from(deleteSelect.options).some(o => o.value);
		if (!hasFiles) deleteSelect.innerHTML = '<option value="">-- No files uploaded yet --</option>';

		setTimeout(() => deleteResult.classList.add("hidden"), 5000);
	} catch (err) {
		deleteResult.textContent = `✗ Error: ${escapeHtml(err.message)}`;
		deleteResult.className = "error";
		deleteResult.classList.remove("hidden");
		console.error("delete error:", err);
		deleteSelectedBtn.disabled = false;
		deleteSelectedBtn.textContent = prevText || "🗑️ Delete Selected";
		setTimeout(() => deleteResult.classList.add("hidden"), 5000);
	}
});

// hook refresh
if (refreshBtn) refreshBtn.addEventListener("click", loadFileListForDelete);

// auto-load
loadFileListForDelete();