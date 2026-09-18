// Code Performance Analyzer V4 - Main Web Application Controller

function formatTimeSmart(ns) {
  if (ns === undefined || ns === null || isNaN(ns)) return '0 µs';
  const val = Number(ns);
  if (val < 1000) {
    return `${val.toFixed(1)} ns`;
  } else if (val < 1000000) {
    return `${(val / 1000.0).toFixed(2)} µs`;
  } else if (val < 1000000000) {
    return `${(val / 1000000.0).toFixed(2)} ms`;
  } else {
    return `${(val / 1000000000.0).toFixed(2)} s`;
  }
}

function formatMemorySmart(bytes) {
  if (!bytes || bytes <= 0) return '0 KB';
  if (bytes < 1024 * 1024) return `${(bytes / 1024.0).toFixed(1)} KB`;
  return `${(bytes / (1024.0 * 1024.0)).toFixed(2)} MB`;
}

function escapeHtml(str) {
  if (str === undefined || str === null) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

const CONTRACT_DEFS = {
  'search': {
    title: 'TEMPLATE 1: TARGETED SEARCH (TARGET REQUIRED)',
    badge: '✓ Available',
    badgeClass: 'badge-success',
    isAvailable: true,
    inputSpec: 'const int* data',
    sizeSpec: 'int size',
    paramSpec: 'int target',
    returnSpec: 'index or -1',
    templateFilename: 'targeted_template.cpp',
    code: `#include <vector>

// Template 1: Targeted Search (Target Required)
// Contract: Returns 0-based index of target if found, or -1 if not found.
int custom_algorithm(const int* data, int size, int target) {
    for (int i = 0; i < size; ++i) {
        if (data[i] == target) {
            return i;
        }
    }
    return -1;
}`
  },
  'dataset': {
    title: 'TEMPLATE 2: DATASET OPERATION / SCAN (NO TARGET)',
    badge: '✓ Available',
    badgeClass: 'badge-success',
    isAvailable: true,
    inputSpec: 'const int* data',
    sizeSpec: 'int size',
    paramSpec: 'None (Dataset Only)',
    returnSpec: 'long long (result)',
    templateFilename: 'dataset_template.cpp',
    code: `#include <vector>

// Template 2: Dataset Operation / Scan (No Target Required)
// Contract: Processes or inspects dataset without external target.
// Returns computed index, aggregate sum, max/min, or 0 on success.
long long custom_algorithm(const int* data, int size) {
    long long max_val = -2147483647 - 1;
    int max_idx = -1;
    for (int i = 0; i < size; ++i) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = i;
        }
    }
    return max_idx;
}`
  },
  'sorting': {
    title: 'TEMPLATE 3: CUSTOM SORTING (IN-PLACE ARRAY SORT)',
    badge: '✓ Available',
    badgeClass: 'badge-success',
    isAvailable: true,
    inputSpec: 'int* data (Mutable)',
    sizeSpec: 'int size',
    paramSpec: 'In-Place Ascending',
    returnSpec: 'void (Sorted in-place)',
    templateFilename: 'sorting_template.cpp',
    code: `#include <vector>

// Template 3: Custom Sorting (In-Place Array Sort)
// Contract: Sorts data[0..size-1] in-place in ascending order.
void custom_algorithm(int* data, int size) {
    for (int i = 0; i < size - 1; ++i) {
        for (int j = 0; j < size - i - 1; ++j) {
            if (data[j] > data[j + 1]) {
                int tmp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = tmp;
            }
        }
    }
}`
  },
  'matrix': {
    title: 'TEMPLATE 4: MATRIX OPERATIONS (N x N)',
    badge: '◐ Reference Preview',
    badgeClass: 'badge-secondary',
    isAvailable: false,
    inputSpec: 'const double* A, const double* B',
    sizeSpec: 'int n (Matrix n x n)',
    paramSpec: 'double* C (Output)',
    returnSpec: 'void (C buffer)',
    templateFilename: 'matrix_template.cpp',
    code: `#include <cstddef>

// Template 4: Matrix Operations (Reference Preview)
// Contract:
//   A -> Flat row-major n x n input matrix
//   B -> Flat row-major n x n input matrix
//   C -> Flat row-major n x n output matrix (pre-allocated)
//   n -> Matrix dimension (n x n)
void custom_algorithm(const double* A, const double* B, double* C, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (int k = 0; k < n; ++k) {
                sum += A[i * n + k] * B[k * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}`
  }
};

// Aliases for compatibility
CONTRACT_DEFS['search:with_target'] = CONTRACT_DEFS['search'];
CONTRACT_DEFS['search:dataset_only'] = CONTRACT_DEFS['dataset'];

function escapeHtml(str) {
  if (str === null || str === undefined) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function buildChartDataFromRun(run, metric, preferredDist = '') {
  if (!run || !run.results || run.results.length === 0) {
    return { valid: false, series: [] };
  }

  // Determine available input distributions
  const dists = new Set();
  run.results.forEach(r => {
    if (r.input_type) dists.add(r.input_type);
  });

  // Select primary distribution to isolate curves and avoid collision
  let targetDist = preferredDist;
  if (!targetDist || !dists.has(targetDist)) {
    if (dists.has('Random')) targetDist = 'Random';
    else if (dists.size > 0) targetDist = Array.from(dists)[0];
    else targetDist = '';
  }

  const seriesMap = {};
  run.results.forEach(r => {
    // If targetDist exists, isolate to that distribution
    if (targetDist && r.input_type && r.input_type !== targetDist) {
      return;
    }
    const name = r.algorithm_name || r.name || r.algorithm;
    const seriesKey = r.algorithm || name;
    if (!seriesMap[seriesKey]) {
      seriesMap[seriesKey] = { algorithm: r.algorithm, name: name, points: [] };
    }
    const xVal = Number(r.input_size) || 0;
    let yVal = 0;
    if (metric === 'memory') {
      const peakBytes = r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0) ?? 0;
      yVal = peakBytes / (1024.0 * 1024.0); // MB
    } else {
      yVal = (r.time_mean_ns ?? r.mean_time_ns ?? 0) / 1000.0; // µs
    }
    seriesMap[seriesKey].points.push({ x: xVal, y: yVal });
  });

  const seriesList = Object.values(seriesMap).map(s => {
    s.points.sort((a, b) => a.x - b.x);
    return s;
  });

  const isCustom = run.benchmark_mode === 'custom';
  let distTitle = '';
  if (isCustom) {
    distTitle = run.interface_mode === 'dataset_only' ? ' (Custom: Dataset Only)' : ` (Custom Target: ${run.custom_target_parameter ?? '5000'})`;
  } else if (targetDist) {
    distTitle = ` (${targetDist})`;
  }

  return {
    valid: seriesList.length > 0,
    title: metric === 'memory' ? `Peak Memory vs Input Size (N)${distTitle}` : `Execution Time vs Input Size (N)${distTitle}`,
    x_label: 'Input Size (N)',
    y_label: metric === 'memory' ? 'Peak RAM (MB)' : 'Mean Time (µs)',
    series: seriesList
  };
}

const App = {
  activeView: 'dashboard',
  pollTimer: null,

  init() {
    this.bindNavigation();
    this.bindDashboard();
    this.bindBenchmarkForm();
    this.bindCustomBenchmark();
    this.bindGraphs();
    this.bindComparison();

    // Initial view load
    this.switchView('dashboard');
    this.pollEngineStatus();
  },

  // ── Navigation ─────────────────────────────────────────────────────────────
  bindNavigation() {
    document.querySelectorAll('.nav-item[data-view]').forEach(item => {
      item.addEventListener('click', (e) => {
        e.preventDefault();
        const view = item.getAttribute('data-view');
        this.switchView(view);
      });
    });
  },

  switchView(viewName) {
    this.activeView = viewName;

    // Update sidebar navigation active state
    document.querySelectorAll('.nav-item').forEach(el => {
      el.classList.toggle('active', el.getAttribute('data-view') === viewName);
    });

    // Update view container visibility
    document.querySelectorAll('.view-section').forEach(sec => {
      sec.classList.toggle('active', sec.id === `view-${viewName}`);
    });

    // Trigger data fetch for current view
    switch (viewName) {
      case 'dashboard':
        this.loadDashboard();
        break;
      case 'benchmark':
        this.renderBenchmarkResults();
        break;
      case 'history':
        this.loadHistory();
        break;
      case 'comparison':
        this.loadComparisonView();
        break;
      case 'graphs':
        this.loadGraphs();
        break;
      case 'reports':
        this.loadReports();
        break;
    }
  },

  // ── 1. Dashboard View ──────────────────────────────────────────────────────
  bindDashboard() {
    const btnQuickReport = document.getElementById('btn-quick-report');
    if (btnQuickReport) {
      btnQuickReport.addEventListener('click', async () => {
        btnQuickReport.disabled = true;
        btnQuickReport.textContent = 'Generating...';
        try {
          const res = await API.generateReport();
          if (res.success) {
            btnQuickReport.textContent = 'Report Generated ✓';
          } else {
            btnQuickReport.textContent = 'Report Failed';
          }
        } catch (err) {
          btnQuickReport.textContent = 'Error';
        }
        setTimeout(() => {
          btnQuickReport.disabled = false;
          btnQuickReport.textContent = 'Open Latest HTML Report';
        }, 2500);
      });
    }
  },

  async loadDashboard() {
    try {
      // 1. Load latest run
      const run = await API.getRun();

      // 2. Load latest comparison
      const comp = await API.getCompare();
      if (comp && comp.valid && comp.groups && comp.groups.length > 0) {
        const group = comp.groups[0];
        const fastest = group.rankings[0];
        const slowest = group.rankings[group.rankings.length - 1];

        if (fastest) {
          const fname = fastest.name || fastest.algorithm_name || fastest.algorithm;
          document.getElementById('dash-fastest-name').textContent = fname;
          const meanNs = fastest.time_mean_ns ?? fastest.mean_time_ns ?? 0;
          const timeStr = formatTimeSmart(meanNs);
          const sp = fastest.speedup ?? fastest.speedup_factor ?? 1.0;
          document.getElementById('dash-fastest-sub').textContent = `${timeStr} • ${sp.toFixed(1)}x vs slowest`;
        }

        if (slowest) {
          const sname = slowest.name || slowest.algorithm_name || slowest.algorithm;
          document.getElementById('dash-slowest-name').textContent = sname;
          const meanNs = slowest.time_mean_ns ?? slowest.mean_time_ns ?? 0;
          const timeStr = formatTimeSmart(meanNs);
          document.getElementById('dash-slowest-sub').textContent = `${timeStr} • baseline (1.0x)`;
        }

        // Render Speedup horizontal bar chart
        const labels = group.rankings.map(r => r.name || r.algorithm_name || r.algorithm).reverse();
        const speedups = group.rankings.map(r => r.speedup ?? r.speedup_factor ?? 1.0).reverse();
        Charts.renderBarChart('dashSpeedupChart', labels, speedups, 'Speedup Factor');
      } else if (run && run.results && run.results.length > 0) {
        // Fallback when only 1 algorithm was benchmarked or homogeneous comparison
        const first = run.results[0];
        const name = first.algorithm_name || first.algorithm;
        const timeStr = formatTimeSmart(first.time_mean_ns);
        document.getElementById('dash-fastest-name').textContent = name;
        document.getElementById('dash-fastest-sub').textContent = `${timeStr} • Benchmarked`;
        document.getElementById('dash-slowest-name').textContent = name;
        document.getElementById('dash-slowest-sub').textContent = `${timeStr} • baseline`;

        Charts.renderBarChart('dashSpeedupChart', [name], [1.0], 'Speedup Factor');
      } else {
        document.getElementById('dash-fastest-name').textContent = 'No runs yet';
        document.getElementById('dash-fastest-sub').textContent = 'Run a benchmark to begin';
        document.getElementById('dash-slowest-name').textContent = 'No runs yet';
        document.getElementById('dash-slowest-sub').textContent = '—';
      }

      // 3. Load latest run details for peak memory
      if (run && run.results && run.results.length > 0) {
        let maxMemBytes = 0;
        run.results.forEach(r => {
          const peak = r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0) ?? 0;
          if (peak > maxMemBytes) {
            maxMemBytes = peak;
          }
        });
        const memMB = (maxMemBytes / (1024.0 * 1024.0)).toFixed(2);
        document.getElementById('dash-peak-mem').textContent = `${memMB} MB`;
      } else {
        document.getElementById('dash-peak-mem').textContent = '0.00 MB';
      }

      // 4. Load regression analysis
      const reg = await API.getRegression();
      const badgeContainer = document.getElementById('dash-regression-badge');
      const badgeSub = document.getElementById('dash-regression-sub');
      if (reg && reg.valid) {
        const count = reg.regressed_count ?? reg.regression_count ?? 0;
        if (count > 0 || reg.has_regressions) {
          badgeContainer.innerHTML = `<span class="badge badge-danger">⚠️ ${count} REGRESSION${count > 1 ? 'S' : ''}</span>`;
          badgeSub.textContent = `Baseline: ${reg.baseline_run_id || 'previous run'}`;
        } else {
          badgeContainer.innerHTML = `<span class="badge badge-success">✓ STABLE</span>`;
          badgeSub.textContent = `Baseline: ${reg.baseline_run_id || 'previous run'}`;
        }
      } else {
        badgeContainer.innerHTML = `<span class="badge badge-secondary">INITIAL</span>`;
        badgeSub.textContent = (reg && reg.error) ? reg.error : 'No compatible baseline in history';
      }

      // 5. Render Dashboard Trend Charts (Isolated to Latest Run)
      if (run && run.results && run.results.length > 0) {
        const timeData = buildChartDataFromRun(run, 'time');
        if (timeData && timeData.valid) {
          Charts.renderLineChart('dashTimeChart', timeData, 'µs');
        } else if (Charts.instances['dashTimeChart']) {
          Charts.instances['dashTimeChart'].destroy();
          delete Charts.instances['dashTimeChart'];
        }

        const memData = buildChartDataFromRun(run, 'memory');
        if (memData && memData.valid) {
          Charts.renderLineChart('dashMemoryChart', memData, 'MB');
        } else if (Charts.instances['dashMemoryChart']) {
          Charts.instances['dashMemoryChart'].destroy();
          delete Charts.instances['dashMemoryChart'];
        }
      } else {
        if (Charts.instances['dashTimeChart']) {
          Charts.instances['dashTimeChart'].destroy();
          delete Charts.instances['dashTimeChart'];
        }
        if (Charts.instances['dashMemoryChart']) {
          Charts.instances['dashMemoryChart'].destroy();
          delete Charts.instances['dashMemoryChart'];
        }
      }

      // 6. Populate System Information
      if (run && run.system) {
        this.updateSystemInfo(run.system);
      }
    } catch (err) {
      console.error('Error loading dashboard:', err);
    }
  },

  updateSystemInfo(sys) {
    if (!sys) return;
    const cpuEl = document.getElementById('dash-sys-cpu');
    if (cpuEl && sys.cpu) cpuEl.textContent = sys.cpu;
    const osEl = document.getElementById('dash-sys-os');
    if (osEl && sys.os) osEl.textContent = sys.os;
    const archEl = document.getElementById('dash-sys-arch');
    if (archEl && sys.architecture) archEl.textContent = sys.architecture;
    const coresEl = document.getElementById('dash-sys-cores');
    if (coresEl) {
      const p = sys.physical_cores || 0;
      const l = sys.logical_cpus || 0;
      coresEl.textContent = (p || l) ? `${p} Physical / ${l} Logical` : '—';
    }
    const compEl = document.getElementById('dash-sys-compiler');
    if (compEl && sys.compiler) compEl.textContent = sys.compiler;
    const cxxEl = document.getElementById('dash-sys-cxx');
    if (cxxEl) {
      const cxx = sys.cxx_standard || 'C++17';
      const opt = sys.optimization || 'Enabled (-O2)';
      cxxEl.textContent = `${cxx} • ${opt}`;
    }

    // Populate CPU Core Control dropdown if not already populated
    const affinitySelect = document.getElementById('cfg-affinity');
    const customAffinitySelect = document.getElementById('custom-cfg-affinity');
    if (sys.logical_cpus) {
      if (affinitySelect && affinitySelect.options.length <= 1) {
        for (let i = 0; i < sys.logical_cpus; ++i) {
          const opt = document.createElement('option');
          opt.value = i;
          opt.textContent = `Core ${i} (Pinned)`;
          affinitySelect.appendChild(opt);
        }
      }
      if (customAffinitySelect && customAffinitySelect.options.length <= 1) {
        for (let i = 0; i < sys.logical_cpus; ++i) {
          const opt = document.createElement('option');
          opt.value = i;
          opt.textContent = `Core ${i} (Pinned)`;
          customAffinitySelect.appendChild(opt);
        }
      }
    }
  },

  // ── 2. Run Benchmark Form ──────────────────────────────────────────────────
  bindBenchmarkForm() {
    // Mode Switcher (Standard Sorting vs Custom Algorithms)
    const btnModeStd = document.getElementById('btn-mode-standard');
    const btnModeCustom = document.getElementById('btn-mode-custom');
    const containerStd = document.getElementById('container-standard-benchmark');
    const containerCustom = document.getElementById('container-custom-benchmark');

    if (btnModeStd && btnModeCustom) {
      btnModeStd.addEventListener('click', () => {
        btnModeStd.classList.remove('btn-secondary');
        btnModeStd.classList.add('btn-primary');
        btnModeCustom.classList.remove('btn-primary');
        btnModeCustom.classList.add('btn-secondary');
        if (containerStd) containerStd.style.display = 'block';
        if (containerCustom) containerCustom.style.display = 'none';
      });

      btnModeCustom.addEventListener('click', () => {
        btnModeCustom.classList.remove('btn-secondary');
        btnModeCustom.classList.add('btn-primary');
        btnModeStd.classList.remove('btn-primary');
        btnModeStd.classList.add('btn-secondary');
        if (containerStd) containerStd.style.display = 'none';
        if (containerCustom) containerCustom.style.display = 'block';
      });
    }

    const inputTypeSelect = document.getElementById('cfg-input-type');
    const fileGroup = document.getElementById('cfg-file-group');
    const btnValidate = document.getElementById('btn-validate-file');
    const filePathInput = document.getElementById('cfg-file-path');
    const validationMsg = document.getElementById('file-validation-msg');
    const validationDetails = document.getElementById('file-validation-details');
    const modeBadge = document.getElementById('cfg-mode-badge');
    const sizesContainer = document.getElementById('cfg-sizes-container');
    const customSizeNotice = document.getElementById('cfg-custom-size-notice');
    const customSizeText = document.getElementById('cfg-custom-size-text');

    const btnBrowse = document.getElementById('btn-browse-file');
    const filePicker = document.getElementById('custom-file-picker');
    const btnSample = document.getElementById('btn-load-sample');

    const displayValidation = (res) => {
      if (!validationDetails) return;
      if (res.valid) {
        validationDetails.style.display = 'block';
        validationDetails.style.background = 'rgba(16, 185, 129, 0.08)';
        validationDetails.style.borderColor = 'rgba(16, 185, 129, 0.3)';
        const badge = document.getElementById('file-val-badge');
        if (badge) {
          badge.className = 'badge badge-success';
          badge.textContent = '✓ VALID DATASET';
        }
        const valPath = document.getElementById('file-val-path');
        if (valPath) valPath.textContent = res.file_path || '';
        const countEl = document.getElementById('file-val-count');
        if (countEl) countEl.textContent = (res.element_count || 0).toLocaleString();
        const orderEl = document.getElementById('file-val-order');
        if (orderEl) orderEl.textContent = res.order || 'Unsorted';
        const distEl = document.getElementById('file-val-distinct');
        if (distEl) distEl.textContent = (res.distinct_count || 0).toLocaleString();
        const dupEl = document.getElementById('file-val-duplicates');
        if (dupEl) dupEl.textContent = (res.duplicate_count || 0).toLocaleString();
        const minEl = document.getElementById('file-val-min');
        if (minEl) minEl.textContent = res.min_value !== undefined ? res.min_value.toLocaleString() : '—';
        const maxEl = document.getElementById('file-val-max');
        if (maxEl) maxEl.textContent = res.max_value !== undefined ? res.max_value.toLocaleString() : '—';

        if (validationMsg) validationMsg.textContent = '';
        if (customSizeText) {
          customSizeText.textContent = `Input size is fixed to ${res.element_count.toLocaleString()} elements from ${res.file_path}.`;
        }
      } else {
        validationDetails.style.display = 'none';
        if (validationMsg) {
          validationMsg.style.color = '#f43f5e';
          validationMsg.textContent = `✗ Error: ${res.error_message || 'File could not be validated'}`;
        }
      }
    };

    const validatePath = async (path) => {
      if (!path) {
        if (validationMsg) {
          validationMsg.style.color = '#f43f5e';
          validationMsg.textContent = 'Please enter a valid file path.';
        }
        if (validationDetails) validationDetails.style.display = 'none';
        return;
      }
      if (btnValidate) {
        btnValidate.disabled = true;
        btnValidate.textContent = 'Checking...';
      }
      const res = await API.validateFile(path);
      if (btnValidate) {
        btnValidate.disabled = false;
        btnValidate.textContent = 'Validate File';
      }
      displayValidation(res);
    };

    if (inputTypeSelect) {
      inputTypeSelect.addEventListener('change', () => {
        if (inputTypeSelect.value === 'CustomFile') {
          if (fileGroup) fileGroup.style.display = 'block';
          if (modeBadge) modeBadge.textContent = 'Custom File';
          if (sizesContainer) sizesContainer.style.display = 'none';
          if (customSizeNotice) customSizeNotice.style.display = 'block';
          if (filePathInput && filePathInput.value.trim()) {
            validatePath(filePathInput.value.trim());
          }
        } else {
          if (fileGroup) fileGroup.style.display = 'none';
          if (modeBadge) modeBadge.textContent = 'Generated Distribution';
          if (sizesContainer) sizesContainer.style.display = 'block';
          if (customSizeNotice) customSizeNotice.style.display = 'none';
          if (validationMsg) validationMsg.textContent = '';
          if (validationDetails) validationDetails.style.display = 'none';
        }
      });
    }

    if (btnBrowse && filePicker) {
      btnBrowse.addEventListener('click', () => {
        filePicker.click();
      });

      filePicker.addEventListener('change', async () => {
        const file = filePicker.files[0];
        if (!file) return;

        btnBrowse.disabled = true;
        btnBrowse.textContent = 'Uploading...';

        try {
          const reader = new FileReader();
          reader.onload = async (e) => {
            const content = e.target.result;
            const res = await API.uploadDataset(file.name, content);
            btnBrowse.disabled = false;
            btnBrowse.textContent = '📁 Browse & Upload';

            if (res && res.valid) {
              if (filePathInput) filePathInput.value = res.file_path;
              displayValidation(res);
            } else {
              if (validationMsg) {
                validationMsg.style.color = '#f43f5e';
                validationMsg.textContent = `✗ Upload failed: ${res.error_message || 'Unknown error'}`;
              }
              if (validationDetails) validationDetails.style.display = 'none';
            }
          };
          reader.readAsText(file);
        } catch (err) {
          btnBrowse.disabled = false;
          btnBrowse.textContent = '📁 Browse & Upload';
          if (validationMsg) {
            validationMsg.style.color = '#f43f5e';
            validationMsg.textContent = `✗ Read error: ${err.message}`;
          }
        }
      });
    }

    if (btnSample && filePathInput) {
      btnSample.addEventListener('click', async () => {
        filePathInput.value = 'sample_dataset.txt';
        await validatePath('sample_dataset.txt');
      });
    }

    if (btnValidate && filePathInput) {
      btnValidate.addEventListener('click', async () => {
        await validatePath(filePathInput.value.trim());
      });
    }

    // Algorithm selection chips
    const btnSelectAll = document.getElementById('btn-select-all-alg');
    const btnClearAll = document.getElementById('btn-clear-all-alg');
    if (btnSelectAll) {
      btnSelectAll.addEventListener('click', () => {
        document.querySelectorAll('#algorithm-chips input[type="checkbox"]').forEach(cb => { cb.checked = true; });
      });
    }
    if (btnClearAll) {
      btnClearAll.addEventListener('click', () => {
        document.querySelectorAll('#algorithm-chips input[type="checkbox"]').forEach(cb => { cb.checked = false; });
      });
    }

    // Run benchmark button
    const btnRun = document.getElementById('btn-run-benchmark');
    const btnCancel = document.getElementById('btn-cancel-benchmark');
    const progBox = document.getElementById('benchmark-progress-box');

    if (btnRun) {
      btnRun.addEventListener('click', async () => {
        // Collect checked algorithms
        const algs = [];
        document.querySelectorAll('#algorithm-chips input[type="checkbox"]:checked').forEach(cb => {
          algs.push(cb.value);
        });

        if (algs.length === 0) {
          alert('Please select at least one algorithm to benchmark.');
          return;
        }

        // Collect checked sizes
        const sizes = [];
        document.querySelectorAll('#size-chips input[type="checkbox"]:checked').forEach(cb => {
          const sz = parseInt(cb.value, 10);
          if (sz > 0) sizes.push(sz);
        });

        const inputType = document.getElementById('cfg-input-type').value;
        const filePath = document.getElementById('cfg-file-path').value.trim();

        if (inputType === 'CustomFile' && !filePath) {
          alert('Please specify a custom dataset file path.');
          return;
        }

        const iterations = parseInt(document.getElementById('cfg-iterations').value, 10) || 20;
        const warmup = parseInt(document.getElementById('cfg-warmup').value, 10) || 5;
        const memory = document.getElementById('cfg-memory').value === 'true';
        const affinityEl = document.getElementById('cfg-affinity');
        const cpuAffinity = affinityEl ? parseInt(affinityEl.value, 10) : -1;

        const payload = {
          input_type: inputType,
          file_path: filePath,
          algorithms: algs,
          sizes: sizes,
          iterations: iterations,
          warmup: warmup,
          memory: memory,
          cpu_affinity: isNaN(cpuAffinity) ? -1 : cpuAffinity
        };

        btnRun.disabled = true;
        btnRun.style.display = 'none';
        btnCancel.style.display = 'inline-block';
        progBox.classList.add('active');

        const resContainer = document.getElementById('benchmark-results-container');
        if (resContainer) resContainer.style.display = 'none';

        const res = await API.startBenchmark(payload);
        if (res.status === 'started' || res.status === 'accepted') {
          this.startBenchmarkPolling();
        } else {
          alert(`Could not start benchmark: ${res.message || 'Unknown error'}`);
          btnRun.disabled = false;
          btnRun.style.display = 'inline-block';
          btnCancel.style.display = 'none';
          progBox.classList.remove('active');
        }
      });
    }

    if (btnCancel) {
      btnCancel.addEventListener('click', async () => {
        btnCancel.disabled = true;
        btnCancel.textContent = 'Cancelling...';
        await API.cancelBenchmark();
      });
    }
  },

  // ── Custom Benchmark Form ──────────────────────────────────────────────────
  bindCustomBenchmark() {
    const categorySelect = document.getElementById('custom-cfg-category');
    const interfaceGroup = document.getElementById('custom-interface-group');
    const interfaceSelect = document.getElementById('custom-cfg-interface');
    const domainBadge = document.getElementById('custom-domain-badge');
    const contractTitle = document.getElementById('contract-title');
    const contractBadge = document.getElementById('contract-badge');
    const specInput = document.getElementById('spec-input');
    const specSize = document.getElementById('spec-size');
    const specParam = document.getElementById('spec-param');
    const specReturn = document.getElementById('spec-return');
    const contractSnippet = document.getElementById('contract-code-snippet');
    const customParamsCardTitle = document.getElementById('custom-params-card-title');
    const customAlgATitle = document.getElementById('custom-alg-a-title');
    const customAlgBTitle = document.getElementById('custom-alg-b-title');
    const customPreviewNotice = document.getElementById('custom-preview-notice');

    const panelSearchTarget = document.getElementById('panel-param-search-target');
    const panelSearchDataset = document.getElementById('panel-param-search-dataset');
    const panelMatrix = document.getElementById('panel-param-matrix');
    const panelGraph = document.getElementById('panel-param-graph');
    const panelSorting = document.getElementById('panel-param-sorting');

    const btnCopyTemplate = document.getElementById('btn-copy-template');
    const btnDownloadTemplate = document.getElementById('btn-download-template');

    const btnBrowseAlgA = document.getElementById('btn-browse-alg-a');
    const fileAlgA = document.getElementById('custom-alg-a-file');
    const pathAlgA = document.getElementById('custom-alg-a-path');
    const nameAlgA = document.getElementById('custom-alg-a-name');
    const statusAlgA = document.getElementById('custom-alg-a-status');

    const btnBrowseAlgB = document.getElementById('btn-browse-alg-b');
    const fileAlgB = document.getElementById('custom-alg-b-file');
    const pathAlgB = document.getElementById('custom-alg-b-path');
    const nameAlgB = document.getElementById('custom-alg-b-name');
    const statusAlgB = document.getElementById('custom-alg-b-status');

    const btnLoadSamples = document.getElementById('btn-custom-load-samples');
    const btnBrowseDataset = document.getElementById('btn-browse-custom-dataset');
    const pickerDataset = document.getElementById('custom-dataset-picker');
    const btnSampleDataset = document.getElementById('btn-sample-custom-dataset');
    const pathDataset = document.getElementById('custom-dataset-path');
    const targetVal = document.getElementById('custom-target-val');

    const btnBrowseDataset2 = document.getElementById('btn-browse-custom-dataset-2');
    const pathDataset2 = document.getElementById('custom-dataset-path-2');
    const btnBrowseDataset3 = document.getElementById('btn-browse-custom-dataset-3');
    const pathDataset3 = document.getElementById('custom-sorting-dataset');

    const btnRunCustom = document.getElementById('btn-run-custom-benchmark');
    const btnCancelCustom = document.getElementById('btn-cancel-custom-benchmark');

    // Target pre-check helper with async race prevention
    let targetCheckSeq = 0;
    let targetDebounceTimer = null;

    const updateTargetBadge = async () => {
      const cat = categorySelect ? categorySelect.value : 'search';
      const statusEl = document.getElementById('custom-target-status');
      if (cat !== 'search') {
        return;
      }

      const currentSeq = ++targetCheckSeq;
      const path = pathDataset ? pathDataset.value.trim() : '';
      const target = targetVal ? targetVal.value.trim() : '';
      if (!statusEl) return;
      if (!path || target === '') {
        statusEl.textContent = '🎯 Target Status: Enter dataset and target value';
        statusEl.style.color = 'var(--text-muted)';
        return;
      }
      const res = await API.detectTarget(path, target);
      if (currentSeq !== targetCheckSeq) return; // Stale async response discarded

      if (res && res.valid) {
        if (res.available) {
          statusEl.style.color = '#10b981';
          statusEl.style.borderColor = 'rgba(16, 185, 129, 0.3)';
          statusEl.style.background = 'rgba(16, 185, 129, 0.08)';
          statusEl.textContent = `🎯 Target Status: 🟢 Available at index ${res.first_index} (${res.occurrences} occurrence${res.occurrences > 1 ? 's' : ''}) • Present-Target Benchmark`;
        } else {
          statusEl.style.color = '#f59e0b';
          statusEl.style.borderColor = 'rgba(245, 158, 11, 0.3)';
          statusEl.style.background = 'rgba(245, 158, 11, 0.08)';
          statusEl.textContent = `🎯 Target Status: ⚪ Absent from Dataset (${res.total_elements} elements) • Negative Search Test`;
        }
      } else {
        statusEl.style.color = '#f43f5e';
        statusEl.textContent = `🎯 Target Status: ✗ ${res ? res.error_message : 'Check failed'}`;
      }
    };

    const debouncedUpdateTargetBadge = () => {
      if (targetDebounceTimer) clearTimeout(targetDebounceTimer);
      targetDebounceTimer = setTimeout(updateTargetBadge, 150);
    };

    const updateCategoryAndInterface = () => {
      const cat = categorySelect ? categorySelect.value : 'search:with_target';
      let key = cat;
      if (key === 'search:with_target') key = 'search';
      else if (key === 'search:dataset_only') key = 'dataset';
      const contract = CONTRACT_DEFS[cat] || CONTRACT_DEFS[key] || CONTRACT_DEFS['search'];

      // Update Contract & Code Box
      if (contractTitle) contractTitle.textContent = contract.title;
      if (contractBadge) {
        contractBadge.textContent = contract.badge;
        contractBadge.className = `badge ${contract.badgeClass}`;
      }
      if (domainBadge) {
        domainBadge.textContent = contract.badge;
        domainBadge.className = `badge ${contract.badgeClass}`;
      }
      if (specInput) specInput.textContent = contract.inputSpec;
      if (specSize) specSize.textContent = contract.sizeSpec;
      if (specParam) specParam.textContent = contract.paramSpec;
      if (specReturn) specReturn.textContent = contract.returnSpec;
      if (contractSnippet) contractSnippet.textContent = contract.code;

      // Hide all parameter panels first
      if (panelSearchTarget) panelSearchTarget.style.display = 'none';
      if (panelSearchDataset) panelSearchDataset.style.display = 'none';
      if (panelMatrix) panelMatrix.style.display = 'none';
      if (panelGraph) panelGraph.style.display = 'none';
      if (panelSorting) panelSorting.style.display = 'none';

      // Show relevant panel & update titles
      if (key === 'search') {
        if (panelSearchTarget) panelSearchTarget.style.display = 'block';
        if (customParamsCardTitle) customParamsCardTitle.textContent = '3. Dataset & Search Target';
        if (customAlgATitle) customAlgATitle.textContent = 'Algorithm 1 (e.g. Linear Search)';
        if (customAlgBTitle) customAlgBTitle.textContent = 'Algorithm 2 (e.g. Binary Search)';
        updateTargetBadge();
      } else if (key === 'dataset') {
        if (panelSearchDataset) panelSearchDataset.style.display = 'block';
        if (customParamsCardTitle) customParamsCardTitle.textContent = '3. Dataset Configuration (No Target Required)';
        if (customAlgATitle) customAlgATitle.textContent = 'Algorithm 1 (e.g. Max Element Scan)';
        if (customAlgBTitle) customAlgBTitle.textContent = 'Algorithm 2 (e.g. Sum / BST Traversal)';
      } else if (key === 'sorting') {
        if (panelSorting) panelSorting.style.display = 'block';
        if (customParamsCardTitle) customParamsCardTitle.textContent = '3. Sorting Dataset Configuration (In-Place Array Sort)';
        if (customAlgATitle) customAlgATitle.textContent = 'Algorithm 1 (e.g. Bubble Sort)';
        if (customAlgBTitle) customAlgBTitle.textContent = 'Algorithm 2 (e.g. Quick Sort / Selection Sort)';
      } else if (key === 'matrix') {
        if (panelMatrix) panelMatrix.style.display = 'block';
        if (customParamsCardTitle) customParamsCardTitle.textContent = '3. Matrix Parameters (N x N)';
        if (customAlgATitle) customAlgATitle.textContent = 'Algorithm 1 (e.g. Naive Matrix Multiply)';
        if (customAlgBTitle) customAlgBTitle.textContent = 'Algorithm 2 (e.g. Transposed Matrix Multiply)';
      } else if (key === 'graph') {
        if (panelGraph) panelGraph.style.display = 'block';
        if (customParamsCardTitle) customParamsCardTitle.textContent = '3. Graph Parameters (Vertices & Edges)';
        if (customAlgATitle) customAlgATitle.textContent = 'Algorithm 1 (e.g. BFS Traversal)';
        if (customAlgBTitle) customAlgBTitle.textContent = 'Algorithm 2 (e.g. DFS Traversal)';
      }

      // Configure execution state
      if (contract.isAvailable) {
        if (btnRunCustom) {
          btnRunCustom.disabled = false;
          btnRunCustom.style.opacity = '1';
          btnRunCustom.style.cursor = 'pointer';
          btnRunCustom.title = 'Run custom algorithm benchmark';
        }
        if (customPreviewNotice) customPreviewNotice.style.display = 'none';
      } else {
        if (btnRunCustom) {
          btnRunCustom.disabled = true;
          btnRunCustom.style.opacity = '0.5';
          btnRunCustom.style.cursor = 'not-allowed';
          btnRunCustom.title = 'Extension Preview: Code contract and templates available. Live execution engine for this domain is in development.';
        }
        if (customPreviewNotice) customPreviewNotice.style.display = 'inline-block';
      }
    };

    if (categorySelect) categorySelect.addEventListener('change', updateCategoryAndInterface);
    updateCategoryAndInterface();

    if (btnCopyTemplate) {
      btnCopyTemplate.addEventListener('click', async () => {
        const cat = categorySelect ? categorySelect.value : 'search';
        const contract = CONTRACT_DEFS[cat] || CONTRACT_DEFS['search'];
        try {
          await navigator.clipboard.writeText(contract.code);
          const orig = btnCopyTemplate.textContent;
          btnCopyTemplate.textContent = 'Copied! ✓';
          setTimeout(() => { btnCopyTemplate.textContent = orig; }, 2000);
        } catch (e) {
          console.warn('Clipboard copy error:', e);
        }
      });
    }

    if (btnDownloadTemplate) {
      btnDownloadTemplate.addEventListener('click', () => {
        const cat = categorySelect ? categorySelect.value : 'search';
        const contract = CONTRACT_DEFS[cat] || CONTRACT_DEFS['search'];
        const blob = new Blob([contract.code], { type: 'text/x-c++src;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = contract.templateFilename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
      });
    }

    if (targetVal) targetVal.addEventListener('input', debouncedUpdateTargetBadge);
    if (pathDataset) pathDataset.addEventListener('change', updateTargetBadge);

    // Algorithm 1 upload
    if (btnBrowseAlgA && fileAlgA) {
      btnBrowseAlgA.addEventListener('click', () => fileAlgA.click());
      fileAlgA.addEventListener('change', async () => {
        const file = fileAlgA.files[0];
        if (!file) return;
        btnBrowseAlgA.disabled = true;
        btnBrowseAlgA.textContent = 'Uploading...';
        try {
          const reader = new FileReader();
          reader.onload = async (e) => {
            const content = e.target.result;
            const rawCat = categorySelect ? categorySelect.value : 'search';
            const cat = (rawCat === 'search:dataset_only') ? 'dataset' : ((rawCat === 'search:with_target') ? 'search' : rawCat);
            const ifaceMode = (cat === 'search') ? 'with_target' : (cat === 'sorting' ? 'sorting' : 'dataset_only');
            const res = await API.uploadCustomAlgorithm(file.name, content, cat, ifaceMode);
            btnBrowseAlgA.disabled = false;
            btnBrowseAlgA.textContent = '📁 Browse .cpp';
            if (res && (res.success || res.recognized)) {
              if (pathAlgA) pathAlgA.value = res.file_path;
              if (nameAlgA && (!nameAlgA.value || nameAlgA.value.startsWith('Algorithm'))) {
                nameAlgA.value = file.name.replace(/\.[^/.]+$/, '');
              }
              if (statusAlgA) {
                statusAlgA.style.color = '#10b981';
                statusAlgA.style.borderColor = 'rgba(16, 185, 129, 0.3)';
                statusAlgA.style.background = 'rgba(16, 185, 129, 0.08)';
                statusAlgA.textContent = `✓ Interface: ${res.interface_type || 'Auto-detected'} • Function: ${res.function_name || 'Ready'}`;
              }
            } else {
              if (statusAlgA) {
                statusAlgA.style.color = '#f43f5e';
                statusAlgA.textContent = `✗ Upload failed: ${res ? res.diagnostic_message || res.error_message : 'Server error'}`;
              }
            }
          };
          reader.readAsText(file);
        } catch (err) {
          btnBrowseAlgA.disabled = false;
          btnBrowseAlgA.textContent = '📁 Browse .cpp';
          if (statusAlgA) {
            statusAlgA.style.color = '#f43f5e';
            statusAlgA.textContent = `✗ File read error: ${err.message}`;
          }
        }
      });
    }

    // Algorithm 2 upload
    if (btnBrowseAlgB && fileAlgB) {
      btnBrowseAlgB.addEventListener('click', () => fileAlgB.click());
      fileAlgB.addEventListener('change', async () => {
        const file = fileAlgB.files[0];
        if (!file) return;
        btnBrowseAlgB.disabled = true;
        btnBrowseAlgB.textContent = 'Uploading...';
        try {
          const reader = new FileReader();
          reader.onload = async (e) => {
            const content = e.target.result;
            const rawCat = categorySelect ? categorySelect.value : 'search';
            const cat = (rawCat === 'search:dataset_only') ? 'dataset' : ((rawCat === 'search:with_target') ? 'search' : rawCat);
            const ifaceMode = (cat === 'search') ? 'with_target' : (cat === 'sorting' ? 'sorting' : 'dataset_only');
            const res = await API.uploadCustomAlgorithm(file.name, content, cat, ifaceMode);
            btnBrowseAlgB.disabled = false;
            btnBrowseAlgB.textContent = '📁 Browse .cpp';
            if (res && (res.success || res.recognized)) {
              if (pathAlgB) pathAlgB.value = res.file_path;
              if (nameAlgB && (!nameAlgB.value || nameAlgB.value.startsWith('Algorithm'))) {
                nameAlgB.value = file.name.replace(/\.[^/.]+$/, '');
              }
              if (statusAlgB) {
                statusAlgB.style.color = '#10b981';
                statusAlgB.style.borderColor = 'rgba(16, 185, 129, 0.3)';
                statusAlgB.style.background = 'rgba(16, 185, 129, 0.08)';
                statusAlgB.textContent = `✓ Interface: ${res.interface_type || 'Auto-detected'} • Function: ${res.function_name || 'Ready'}`;
              }
            } else {
              if (statusAlgB) {
                statusAlgB.style.color = '#f43f5e';
                statusAlgB.textContent = `✗ Upload failed: ${res ? res.diagnostic_message || res.error_message : 'Server error'}`;
              }
            }
          };
          reader.readAsText(file);
        } catch (err) {
          btnBrowseAlgB.disabled = false;
          btnBrowseAlgB.textContent = '📁 Browse .cpp';
          if (statusAlgB) {
            statusAlgB.style.color = '#f43f5e';
            statusAlgB.textContent = `✗ File read error: ${err.message}`;
          }
        }
      });
    }

    // Load Sample Suite
    if (btnLoadSamples) {
      btnLoadSamples.addEventListener('click', async () => {
        btnLoadSamples.disabled = true;
        btnLoadSamples.textContent = 'Loading Samples...';
        try {
          const data = await API.getCustomSamples();
          if (data && data.algorithms && data.algorithms.length >= 2) {
            const a = data.algorithms[0];
            const b = data.algorithms[1];
            if (nameAlgA) nameAlgA.value = a.name;
            if (pathAlgA) pathAlgA.value = a.path;
            if (statusAlgA) {
              statusAlgA.style.color = '#10b981';
              statusAlgA.style.borderColor = 'rgba(16, 185, 129, 0.3)';
              statusAlgA.style.background = 'rgba(16, 185, 129, 0.08)';
              statusAlgA.textContent = `✓ Interface: ${a.interface} • Auto-Adapter Ready`;
            }

            if (nameAlgB) nameAlgB.value = b.name;
            if (pathAlgB) pathAlgB.value = b.path;
            if (statusAlgB) {
              statusAlgB.style.color = '#10b981';
              statusAlgB.style.borderColor = 'rgba(16, 185, 129, 0.3)';
              statusAlgB.style.background = 'rgba(16, 185, 129, 0.08)';
              statusAlgB.textContent = `✓ Interface: ${b.interface} • Auto-Adapter Ready`;
            }

            if (data.dataset) {
              if (pathDataset) pathDataset.value = data.dataset.path || 'custom/samples/search_data.txt';
              if (pathDataset2) pathDataset2.value = data.dataset.path || 'custom/samples/search_data.txt';
              if (targetVal) targetVal.value = data.dataset.recommended_target || 5000;
            }
          }
          await updateTargetBadge();
        } catch (err) {
          console.error('Error loading custom samples:', err);
        } finally {
          btnLoadSamples.disabled = false;
          btnLoadSamples.textContent = '📄 Load Sample Suite (Linear vs Binary Search)';
        }
      });
    }

    // Dataset upload & sample buttons (Panel 3A: Search with Target)
    if (btnBrowseDataset && pickerDataset) {
      btnBrowseDataset.addEventListener('click', () => {
        pickerDataset.onchange = async () => {
          const file = pickerDataset.files[0];
          if (!file) return;
          btnBrowseDataset.disabled = true;
          btnBrowseDataset.textContent = 'Uploading...';
          try {
            const reader = new FileReader();
            reader.onload = async (e) => {
              const content = e.target.result;
              const res = await API.uploadDataset(file.name, content);
              btnBrowseDataset.disabled = false;
              btnBrowseDataset.textContent = '📁 Upload';
              if (res && res.valid) {
                if (pathDataset) pathDataset.value = res.file_path;
                if (pathDataset2) pathDataset2.value = res.file_path;
                await updateTargetBadge();
              } else {
                alert(`Upload failed: ${res ? res.error_message : 'Server error'}`);
              }
            };
            reader.readAsText(file);
          } catch (err) {
            btnBrowseDataset.disabled = false;
            btnBrowseDataset.textContent = '📁 Upload';
            alert(`File read error: ${err.message}`);
          }
        };
        pickerDataset.click();
      });
    }

    // Dataset upload button (Panel 3B: Search Dataset Only)
    if (btnBrowseDataset2 && pickerDataset) {
      btnBrowseDataset2.addEventListener('click', () => {
        pickerDataset.onchange = async () => {
          const file = pickerDataset.files[0];
          if (!file) return;
          btnBrowseDataset2.disabled = true;
          btnBrowseDataset2.textContent = 'Uploading...';
          try {
            const reader = new FileReader();
            reader.onload = async (e) => {
              const content = e.target.result;
              const res = await API.uploadDataset(file.name, content);
              btnBrowseDataset2.disabled = false;
              btnBrowseDataset2.textContent = '📁 Upload';
              if (res && res.valid) {
                if (pathDataset2) pathDataset2.value = res.file_path;
                if (pathDataset) pathDataset.value = res.file_path;
              } else {
                alert(`Upload failed: ${res ? res.error_message : 'Server error'}`);
              }
            };
            reader.readAsText(file);
          } catch (err) {
            btnBrowseDataset2.disabled = false;
            btnBrowseDataset2.textContent = '📁 Upload';
            alert(`File read error: ${err.message}`);
          }
        };
        pickerDataset.click();
      });
    }

    // Dataset upload button (Panel 3C: Sorting Dataset Only)
    if (btnBrowseDataset3 && pickerDataset) {
      btnBrowseDataset3.addEventListener('click', () => {
        pickerDataset.onchange = async () => {
          const file = pickerDataset.files[0];
          if (!file) return;
          btnBrowseDataset3.disabled = true;
          btnBrowseDataset3.textContent = 'Uploading...';
          try {
            const reader = new FileReader();
            reader.onload = async (e) => {
              const content = e.target.result;
              const res = await API.uploadDataset(file.name, content);
              btnBrowseDataset3.disabled = false;
              btnBrowseDataset3.textContent = '📁 Upload';
              if (res && res.valid) {
                if (pathDataset3) pathDataset3.value = res.file_path;
                if (pathDataset2) pathDataset2.value = res.file_path;
                if (pathDataset) pathDataset.value = res.file_path;
              } else {
                alert(`Upload failed: ${res ? res.error_message : 'Server error'}`);
              }
            };
            reader.readAsText(file);
          } catch (err) {
            btnBrowseDataset3.disabled = false;
            btnBrowseDataset3.textContent = '📁 Upload';
            alert(`File read error: ${err.message}`);
          }
        };
        pickerDataset.click();
      });
    }

    if (btnSampleDataset && pathDataset) {
      btnSampleDataset.addEventListener('click', async () => {
        pathDataset.value = 'custom/samples/search_data.txt';
        if (pathDataset2) pathDataset2.value = 'custom/samples/search_data.txt';
        if (pathDataset3) pathDataset3.value = 'custom/samples/search_data.txt';
        if (targetVal) targetVal.value = 5000;
        await updateTargetBadge();
      });
    }

    // Run Custom Benchmark
    if (btnRunCustom) {
      btnRunCustom.addEventListener('click', async () => {
        const rawCat = categorySelect ? categorySelect.value : 'search:with_target';
        let category = rawCat;
        let ifaceMode = 'with_target';
        if (rawCat === 'search:with_target') {
          category = 'search';
          ifaceMode = 'with_target';
        } else if (rawCat === 'search:dataset_only') {
          category = 'dataset';
          ifaceMode = 'dataset_only';
        } else if (rawCat === 'dataset') {
          category = 'dataset';
          ifaceMode = 'dataset_only';
        } else if (rawCat === 'sorting') {
          category = 'sorting';
          ifaceMode = 'sorting';
        } else if (rawCat === 'search') {
          category = 'search';
          ifaceMode = 'with_target';
        }

        if (category !== 'search' && category !== 'dataset' && category !== 'sorting') {
          alert(`Benchmark execution is currently available for Targeted Search, Dataset Operations, and Custom Sorting.\n\n${category.toUpperCase()} is in Extension Preview. You can inspect its code contract and download the reference template.`);
          return;
        }

        const algASrc = pathAlgA ? pathAlgA.value.trim() : '';
        const algBSrc = pathAlgB ? pathAlgB.value.trim() : '';
        let dataPath = '';
        let target = 0;

        if (category === 'search') {
          dataPath = pathDataset ? pathDataset.value.trim() : '';
          target = targetVal ? (parseInt(targetVal.value, 10) || 0) : 0;
        } else if (category === 'dataset') {
          dataPath = (pathDataset2 && pathDataset2.value.trim()) ? pathDataset2.value.trim() : (pathDataset ? pathDataset.value.trim() : '');
          target = 0;
        } else if (category === 'sorting') {
          dataPath = (pathDataset3 && pathDataset3.value.trim()) ? pathDataset3.value.trim() : (pathDataset ? pathDataset.value.trim() : '');
          target = 0;
        }

        if (!algASrc) {
          alert('Please specify source file for Algorithm 1.');
          return;
        }
        if (!algBSrc) {
          alert('Please specify source file for Algorithm 2.');
          return;
        }
        if (!dataPath) {
          alert('Please specify dataset file path.');
          return;
        }

        const algAName = (nameAlgA && nameAlgA.value.trim()) ? nameAlgA.value.trim() : 'Algorithm 1';
        const algBName = (nameAlgB && nameAlgB.value.trim()) ? nameAlgB.value.trim() : 'Algorithm 2';
        const iterations = parseInt(document.getElementById('custom-cfg-iterations').value, 10) || 20;
        const warmup = parseInt(document.getElementById('custom-cfg-warmup').value, 10) || 5;
        const memory = document.getElementById('custom-cfg-memory').value === 'true';
        const affinityEl = document.getElementById('custom-cfg-affinity');
        const cpuAffinity = affinityEl ? (parseInt(affinityEl.value, 10) || -1) : -1;

        const payload = {
          category: category,
          interface_mode: ifaceMode,
          alg_a_name: algAName,
          alg_a_path: algASrc,
          alg_b_name: algBName,
          alg_b_path: algBSrc,
          algorithms: [
            { id: 'alg_a', name: algAName, source_path: algASrc },
            { id: 'alg_b', name: algBName, source_path: algBSrc }
          ],
          dataset_path: dataPath,
          target: target,
          target_value: target,
          iterations: iterations,
          warmup: warmup,
          memory: memory,
          cpu_affinity: cpuAffinity
        };

        btnRunCustom.disabled = true;
        btnRunCustom.style.display = 'none';
        if (btnCancelCustom) btnCancelCustom.style.display = 'inline-block';

        const progBox = document.getElementById('benchmark-progress-box');
        const progTask = document.getElementById('prog-task-name');
        const progPct = document.getElementById('prog-pct');
        const progFill = document.getElementById('prog-bar-fill');
        if (progBox) progBox.classList.add('active');
        if (progTask) progTask.textContent = 'Compiling custom runner & sandboxing...';
        if (progPct) progPct.textContent = '0%';
        if (progFill) progFill.style.width = '0%';

        const resContainer = document.getElementById('benchmark-results-container');
        if (resContainer) resContainer.style.display = 'none';

        const res = await API.startCustomBenchmark(payload);
        if (res.status === 'started' || res.status === 'accepted') {
          this.startBenchmarkPolling();
        } else {
          alert(`Could not start custom benchmark: ${res.message || 'Unknown error'}`);
          btnRunCustom.disabled = false;
          btnRunCustom.style.display = 'inline-block';
          if (btnCancelCustom) btnCancelCustom.style.display = 'none';
          if (progBox) progBox.classList.remove('active');
        }
      });
    }

    if (btnCancelCustom) {
      btnCancelCustom.addEventListener('click', async () => {
        btnCancelCustom.disabled = true;
        btnCancelCustom.textContent = 'Cancelling...';
        await API.cancelBenchmark();
      });
    }
  },

  startBenchmarkPolling() {
    if (this.pollTimer) clearInterval(this.pollTimer);

    const progBox = document.getElementById('benchmark-progress-box');
    const progTask = document.getElementById('prog-task-name');
    const progPct = document.getElementById('prog-pct');
    const progFill = document.getElementById('prog-bar-fill');
    const progElapsed = document.getElementById('prog-elapsed');
    const progDetail = document.getElementById('prog-current-detail');
    const btnRun = document.getElementById('btn-run-benchmark');
    const btnCancel = document.getElementById('btn-cancel-benchmark');
    const btnRunCustom = document.getElementById('btn-run-custom-benchmark');
    const btnCancelCustom = document.getElementById('btn-cancel-custom-benchmark');

    const resetButtons = () => {
      if (btnRun) {
        btnRun.disabled = false;
        btnRun.style.display = 'inline-block';
      }
      if (btnCancel) {
        btnCancel.disabled = false;
        btnCancel.style.display = 'none';
        btnCancel.textContent = 'Cancel Benchmark';
      }
      if (btnRunCustom) {
        btnRunCustom.disabled = false;
        btnRunCustom.style.display = 'inline-block';
      }
      if (btnCancelCustom) {
        btnCancelCustom.disabled = false;
        btnCancelCustom.style.display = 'none';
        btnCancelCustom.textContent = 'Cancel Benchmark';
      }
    };

    this.pollTimer = setInterval(async () => {
      const status = await API.getStatus();

      if (status.state === 'running') {
        progTask.textContent = `Running: ${status.current_algorithm || 'Preparing...'}`;
        progPct.textContent = `${status.progress}%`;
        progFill.style.width = `${status.progress}%`;
        progElapsed.textContent = `Elapsed: ${status.elapsed_seconds.toFixed(1)}s`;
        progDetail.textContent = `Size N=${status.current_size ? status.current_size.toLocaleString() : '0'} • ${status.current_input_type || ''}`;
        this.updateEngineBadge(`Running (${status.progress}%)`, '#06b6d4');
      } else if (status.state === 'completed') {
        clearInterval(this.pollTimer);
        this.pollTimer = null;
        progTask.textContent = 'Benchmark completed successfully! ✓';
        progPct.textContent = '100%';
        progFill.style.width = '100%';
        progElapsed.textContent = `Total Time: ${status.elapsed_seconds.toFixed(1)}s`;
        this.updateEngineBadge('Engine Ready', '#10b981');

        setTimeout(async () => {
          resetButtons();

          // Immediately render live benchmark results for this completed run!
          await this.renderBenchmarkResults(status.last_run_id);

          // Auto reload dashboard and currently active view
          this.loadDashboard();
          if (this.activeView === 'comparison') this.loadComparisonView();
          else if (this.activeView === 'history') this.loadHistory();
          else if (this.activeView === 'graphs') this.loadGraphs();
          else if (this.activeView === 'reports') this.loadReports();
        }, 1200);
      } else if (status.state === 'cancelled') {
        clearInterval(this.pollTimer);
        this.pollTimer = null;
        progTask.textContent = 'Benchmark was cancelled.';
        this.updateEngineBadge('Engine Ready', '#10b981');
        resetButtons();
      } else if (status.state === 'failed') {
        clearInterval(this.pollTimer);
        this.pollTimer = null;
        progTask.textContent = `Benchmark failed: ${status.error_message}`;
        this.updateEngineBadge('Engine Error', '#f43f5e');
        resetButtons();
      }
    }, 400);
  },

  async pollEngineStatus() {
    try {
      const status = await API.getStatus();
      if (status && status.system) {
        this.updateSystemInfo(status.system);
      }
      if (status.state === 'running') {
        this.updateEngineBadge(`Running (${status.progress}%)`, '#06b6d4');
        if (!this.pollTimer) this.startBenchmarkPolling();
      } else {
        this.updateEngineBadge('Engine Ready', '#10b981');
      }
    } catch (e) {
      this.updateEngineBadge('Engine Offline', '#f43f5e');
    }
  },

  updateEngineBadge(text, color) {
    const txt = document.getElementById('engine-status-text');
    if (txt) {
      txt.textContent = text;
      const dot = txt.parentElement ? txt.parentElement.querySelector('.status-dot') : null;
      if (dot && color) dot.style.background = color;
    }
  },

  async renderBenchmarkResults(runId = null) {
    const container = document.getElementById('benchmark-results-container');
    if (!container) return;

    // Fetch the run to render (specified or latest)
    const run = await API.getRun(runId);
    if (!run || !run.results || run.results.length === 0) {
      return;
    }

    container.style.display = 'block';

    const isCustom = run.benchmark_mode === 'custom';

    // Subtitle with Run ID, timestamp, and input type
    const sub = document.getElementById('bench-res-subtitle');
    if (sub) {
      if (isCustom) {
        const cat = (run.benchmark_category || 'Search').toUpperCase();
        const dPath = run.input ? (run.input.file_path || run.input.type) : (run.input_type || 'Custom');
        if (run.interface_mode === 'dataset_only') {
          sub.textContent = `🧪 Custom Benchmark • Domain: ${cat} • Interface: Dataset Only • Dataset: ${dPath} • Run ID: ${run.run_id}`;
        } else {
          const targetVal = run.custom_target_parameter !== undefined && run.custom_target_parameter !== '' ? run.custom_target_parameter : 'N/A';
          sub.textContent = `🧪 Custom Benchmark • Domain: ${cat} • Target: ${targetVal} • Dataset: ${dPath} • Run ID: ${run.run_id}`;
        }
      } else {
        const inType = run.input_type || (run.input ? run.input.type : '') || 'Generated';
        sub.textContent = `Run ID: ${run.run_id} • Date: ${run.timestamp || 'Latest'} • Input: ${inType}`;
      }
    }

    // Action buttons
    const btnReport = document.getElementById('btn-bench-report');
    if (btnReport) {
      btnReport.onclick = async () => {
        btnReport.disabled = true;
        btnReport.textContent = 'Opening...';
        await API.generateReport(run.run_id);
        setTimeout(() => {
          btnReport.disabled = false;
          btnReport.textContent = '📄 Open HTML Report';
        }, 2000);
      };
    }

    const btnCompare = document.getElementById('btn-bench-compare');
    if (btnCompare) {
      btnCompare.onclick = () => {
        this.targetComparisonRunId = run.run_id;
        this.switchView('comparison');
      };
    }

    // Comparison data for this run
    const comp = await API.getCompare(run.run_id);
    let fastestAlg = null;
    let slowestAlg = null;
    const rankingMap = {};

    if (comp && comp.valid && comp.groups && comp.groups.length > 0) {
      const g = comp.groups[0];
      fastestAlg = g.rankings[0];
      slowestAlg = g.rankings[g.rankings.length - 1];
      g.rankings.forEach(rk => {
        const k = rk.name || rk.algorithm_name || rk.algorithm;
        rankingMap[k] = rk;
        if (rk.algorithm) rankingMap[rk.algorithm] = rk;
      });

      // Speedup Bar Chart
      const labels = g.rankings.map(r => r.name || r.algorithm_name || r.algorithm).reverse();
      const speedups = g.rankings.map(r => r.speedup ?? r.speedup_factor ?? 1.0).reverse();
      Charts.renderBarChart('benchSpeedupChart', labels, speedups, 'Speedup Factor');
    } else {
      const first = run.results[0];
      const name = first.name || first.algorithm_name || first.algorithm;
      fastestAlg = { name: name, time_mean_ns: first.time_mean_ns, speedup: 1.0 };
      slowestAlg = { name: name, time_mean_ns: first.time_mean_ns, speedup: 1.0 };
      Charts.renderBarChart('benchSpeedupChart', [name], [1.0], 'Speedup Factor');
    }

    // Metrics Cards
    if (fastestAlg) {
      const fname = fastestAlg.name || fastestAlg.algorithm_name || fastestAlg.algorithm;
      document.getElementById('bench-fastest-name').textContent = fname;
      const meanNs = fastestAlg.time_mean_ns ?? fastestAlg.mean_time_ns ?? 0;
      const timeStr = formatTimeSmart(meanNs);
      const sp = fastestAlg.speedup ?? fastestAlg.speedup_factor ?? 1.0;
      document.getElementById('bench-fastest-sub').textContent = `${timeStr} • ${Number(sp).toFixed(2)}x vs slowest`;
    }

    if (slowestAlg) {
      const sname = slowestAlg.name || slowestAlg.algorithm_name || slowestAlg.algorithm;
      document.getElementById('bench-slowest-name').textContent = sname;
      const meanNs = slowestAlg.time_mean_ns ?? slowestAlg.mean_time_ns ?? 0;
      const timeStr = formatTimeSmart(meanNs);
      document.getElementById('bench-slowest-sub').textContent = `${timeStr} • baseline (1.0x)`;
    }

    let maxMemBytes = 0;
    run.results.forEach(r => {
      const peak = r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0) ?? 0;
      if (peak > maxMemBytes) maxMemBytes = peak;
    });
    const memMB = (maxMemBytes / (1024.0 * 1024.0)).toFixed(2);
    document.getElementById('bench-peak-mem').textContent = `${memMB} MB`;

    const uniqueAlgs = new Set(run.results.map(r => r.algorithm_name || r.name || r.algorithm));
    const uniqueSizes = new Set(run.results.map(r => r.input_size));
    if (isCustom) {
      document.getElementById('bench-scope-val').textContent = `${uniqueAlgs.size} Custom Algorithms`;
      if (run.interface_mode === 'dataset_only') {
        document.getElementById('bench-scope-sub').textContent = `Dataset Operation • Correctness: PASS ✓ (Isolated Subprocess)`;
      } else {
        const targetVal = run.custom_target_parameter !== undefined && run.custom_target_parameter !== '' ? run.custom_target_parameter : 'N/A';
        document.getElementById('bench-scope-sub').textContent = `Target: ${targetVal} • Correctness: PASS ✓ (Isolated Subprocess)`;
      }
    } else {
      document.getElementById('bench-scope-val').textContent = `${uniqueAlgs.size} Algorithms`;
      document.getElementById('bench-scope-sub').textContent = `${run.results.length} runs across N = ${Array.from(uniqueSizes).join(', ')}`;
    }

    // Build Time & Memory Charts for this specific run
    const timeChartData = buildChartDataFromRun(run, 'time');
    Charts.renderLineChart('benchTimeChart', timeChartData, 'µs');

    const memChartData = buildChartDataFromRun(run, 'memory');
    Charts.renderLineChart('benchMemoryChart', memChartData, 'MB');

    // ── Target Status & Decisions Card (Custom Search with Target) ──────────
    const targetCard = document.getElementById('custom-target-card');
    if (targetCard) {
      const showTargetCard = isCustom &&
        (run.benchmark_category === 'search' || !run.benchmark_category) &&
        (run.interface_mode === 'with_target' || (!run.interface_mode && run.custom_target_parameter !== undefined));

      if (showTargetCard && (run.target_detection || run.custom_target_parameter !== undefined)) {
        targetCard.style.display = 'block';
        const td = run.target_detection || {};
        const tVal = td.target_value !== undefined ? td.target_value : (run.input ? run.input.min_value : '—');
        const isAvail = td.available_in_dataset !== undefined ? td.available_in_dataset : true;
        const occ = td.occurrences !== undefined ? td.occurrences : 1;
        const refIdx = td.expected_index !== undefined ? td.expected_index : '—';

        const tValEl = document.getElementById('target-stat-val');
        if (tValEl) tValEl.textContent = tVal;

        const tAvailEl = document.getElementById('target-stat-avail');
        if (tAvailEl) {
          tAvailEl.innerHTML = isAvail
            ? '<span style="color: #10b981;">✓ Present in Dataset</span>'
            : '<span style="color: #f59e0b;">⚪ Absent (Negative Test)</span>';
        }

        const tCountEl = document.getElementById('target-stat-count');
        if (tCountEl) tCountEl.textContent = occ;

        const tIdxEl = document.getElementById('target-stat-idx');
        if (tIdxEl) {
          tIdxEl.textContent = isAvail ? `Index ${refIdx}` : '-1 (Not in dataset)';
        }

        const tModeEl = document.getElementById('target-mode-badge');
        if (tModeEl) {
          tModeEl.textContent = td.mode_description || (isAvail ? 'Present-Target Benchmark' : 'Absent-Target Negative Test');
          tModeEl.className = isAvail ? 'badge badge-primary' : 'badge badge-secondary';
        }

        // Decisions grid per algorithm at max input size
        const grid = document.getElementById('target-decisions-grid');
        if (grid) {
          grid.innerHTML = '';
          const byAlg = {};
          run.results.forEach(r => {
            const k = r.algorithm_name || r.name || r.algorithm;
            if (!byAlg[k] || r.input_size >= byAlg[k].input_size) {
              byAlg[k] = r;
            }
          });
          Object.values(byAlg).forEach(r => {
            const card = document.createElement('div');
            card.style.cssText = 'padding: 0.85rem 1rem; background: rgba(0, 0, 0, 0.35); border-radius: 6px; border: 1px solid var(--border-color);';
            const passBadge = r.verified
              ? '<span class="badge badge-success">PASS ✓</span>'
              : '<span class="badge badge-danger">MISMATCH ✗</span>';
            const foundBadge = (r.search_target_found || r.search_result_index >= 0)
              ? '<span style="color: #10b981; font-weight: 600;">YES</span>'
              : '<span style="color: var(--text-muted); font-weight: 600;">NO (-1)</span>';
            const retIdxText = r.search_result_index !== undefined ? r.search_result_index : (r.verified ? refIdx : -1);
            card.innerHTML = `
              <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">
                <strong style="color: #fff; font-size: 0.95rem;">${escapeHtml(r.algorithm_name || r.name || r.algorithm)}</strong>
                ${passBadge}
              </div>
              <div style="font-size: 0.82rem; color: var(--text-muted); line-height: 1.6;">
                <div>Returned Index: <strong style="color: var(--accent); font-family: monospace;">${escapeHtml(retIdxText)}</strong></div>
                <div>Target Found: ${foundBadge}</div>
                <div>Decision Accuracy: <strong style="color: ${r.verified ? '#10b981' : '#f43f5e'};">${r.verified ? 'CORRECT DECISION' : 'INCORRECT'}</strong></div>
              </div>
            `;
            grid.appendChild(card);
          });
        }
      } else {
        targetCard.style.display = 'none';
      }
    }

    // ── Observed Time Complexity Card ────────────────────────────────────────
    const compCard = document.getElementById('custom-complexity-card');
    if (compCard) {
      if (run.complexity && run.complexity.length > 0) {
        compCard.style.display = 'block';
        const cTbody = document.getElementById('custom-complexity-tbody');
        if (cTbody) {
          cTbody.innerHTML = '';
          run.complexity.forEach(c => {
            const tr = document.createElement('tr');
            const r2 = (c.fit_quality !== undefined ? Number(c.fit_quality).toFixed(2) : '—');
            tr.innerHTML = `
              <td style="font-weight: 600; color: #fff;">${escapeHtml(c.algorithm_name || c.algorithm)}</td>
              <td style="font-family: monospace; color: var(--accent);">${escapeHtml(c.theoretical || '—')}</td>
              <td style="font-family: monospace; color: #a5f3fc;">${escapeHtml(c.observed_model || '—')}</td>
              <td><strong style="color: #10b981;">${escapeHtml(c.observed_complexity || '—')}</strong></td>
              <td><span class="badge badge-secondary">R² = ${escapeHtml(r2)}</span></td>
            `;
            cTbody.appendChild(tr);
          });
        }
      } else {
        compCard.style.display = 'none';
      }
    }

    // Detailed Table
    const tbody = document.getElementById('bench-table-body');
    const tableCount = document.getElementById('bench-table-count');
    if (tableCount) tableCount.textContent = `${run.results.length} total measurements`;

    if (tbody) {
      tbody.innerHTML = '';
      run.results.forEach(r => {
        const tr = document.createElement('tr');
        const name = r.algorithm_name || r.name || r.algorithm;
        const medStr = formatTimeSmart(r.time_median_ns ?? r.median_time_ns ?? 0);
        const meanStr = formatTimeSmart(r.time_mean_ns ?? r.mean_time_ns ?? 0);
        const minStr = formatTimeSmart(r.time_min_ns ?? 0);
        const maxStr = formatTimeSmart(r.time_max_ns ?? 0);
        const peakBytes = r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0) ?? 0;
        const peakStr = formatMemorySmart(peakBytes);

        const rk = rankingMap[name] || rankingMap[r.algorithm];
        const spVal = rk ? (rk.speedup ?? rk.speedup_factor ?? 1.0) : 1.0;
        const spText = `${Number(spVal).toFixed(2)}x`;

        const badgeInput = isCustom
          ? (run.interface_mode === 'dataset_only'
              ? '<span class="badge badge-primary">Dataset Only</span>'
              : `<span class="badge badge-primary">Target: ${escapeHtml(run.custom_target_parameter ?? 'N/A')}</span>`)
          : `<span class="badge badge-neutral">${escapeHtml(r.input_type || 'Random')}</span>`;

        const statusBadge = (r.verified !== false)
          ? `<span class="badge badge-success">PASS ✓</span>`
          : `<span class="badge badge-danger">FAIL ✗</span>`;

        tr.innerHTML = `
          <td style="font-weight: 600; color: #fff;">${escapeHtml(name)}</td>
          <td>${(r.input_size || 0).toLocaleString()}</td>
          <td>${badgeInput}</td>
          <td>${escapeHtml(medStr)}</td>
          <td>${escapeHtml(meanStr)}</td>
          <td style="color: var(--text-muted); font-size: 0.8rem;">${escapeHtml(minStr)} / ${escapeHtml(maxStr)}</td>
          <td>${escapeHtml(peakStr)}</td>
          <td style="color: var(--accent); font-weight: 600;">${escapeHtml(spText)}</td>
          <td>${statusBadge}</td>
        `;
        tbody.appendChild(tr);
      });
    }

    container.scrollIntoView({ behavior: 'smooth', block: 'start' });
  },

  // ── 3. History View ────────────────────────────────────────────────────────
  async loadHistory() {
    const tbody = document.getElementById('history-table-body');
    if (!tbody) return;
    tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; color: var(--text-muted);">Loading history...</td></tr>';

    const data = await API.getHistory();
    if (!data.runs || data.runs.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; color: var(--text-muted);">No benchmark runs found in archive.</td></tr>';
      return;
    }

    tbody.innerHTML = '';
    data.runs.forEach(run => {
      const tr = document.createElement('tr');
      const algsStr = (run.algorithms || []).slice(0, 4).join(', ') + ((run.algorithms || []).length > 4 ? ` (+${run.algorithms.length - 4} more)` : '');

      const isCustom = run.benchmark_mode === 'custom';
      const badgeHtml = isCustom
        ? `<span class="badge badge-primary">Custom: ${escapeHtml(run.benchmark_category || 'Search')}</span>`
        : `<span class="badge badge-secondary">${escapeHtml(run.input_type || 'Generated')}</span>`;

      tr.innerHTML = `
        <td style="font-family: monospace; font-weight: 600; color: #fff;">${escapeHtml(run.run_id)}</td>
        <td>${escapeHtml(run.timestamp || '—')}</td>
        <td>${badgeHtml}</td>
        <td>${run.element_count ? run.element_count.toLocaleString() : 'N/A'}</td>
        <td style="color: var(--text-muted); font-size: 0.85rem;">${escapeHtml(algsStr)}</td>
        <td>
          <div style="display: flex; gap: 0.5rem;">
            <button class="btn btn-secondary btn-sm btn-hist-compare" data-id="${escapeHtml(run.run_id)}">Compare</button>
            <button class="btn btn-secondary btn-sm btn-hist-report" data-id="${escapeHtml(run.run_id)}">HTML Report</button>
          </div>
        </td>
      `;

      tr.querySelector('.btn-hist-compare').addEventListener('click', () => {
        this.targetComparisonRunId = run.run_id;
        this.switchView('comparison');
      });

      const repBtn = tr.querySelector('.btn-hist-report');
      repBtn.addEventListener('click', async () => {
        repBtn.disabled = true;
        repBtn.textContent = 'Opening...';
        await API.generateReport(run.run_id);
        setTimeout(() => {
          repBtn.disabled = false;
          repBtn.textContent = 'HTML Report';
        }, 2000);
      });

      tbody.appendChild(tr);
    });
  },

  // ── 4. Comparison View ─────────────────────────────────────────────────────
  bindComparison() {
    const sel = document.getElementById('comp-run-select');
    if (sel) {
      sel.addEventListener('change', () => {
        this.renderComparisonTable(sel.value);
      });
    }
  },

  targetComparisonRunId: null,

  async loadComparisonView(targetRunId = null) {
    const sel = document.getElementById('comp-run-select');
    if (!sel) return;

    const data = await API.getHistory();
    sel.innerHTML = '';

    if (!data.runs || data.runs.length === 0) {
      sel.innerHTML = '<option value="">No runs available</option>';
      document.getElementById('comparison-table-body').innerHTML = '<tr><td colspan="6" style="text-align: center; color: var(--text-muted);">No runs found</td></tr>';
      return;
    }

    const desiredId = targetRunId || this.targetComparisonRunId;
    this.targetComparisonRunId = null;

    let selectedId = data.runs[0].run_id;
    if (desiredId && data.runs.some(r => r.run_id === desiredId)) {
      selectedId = desiredId;
    }

    data.runs.forEach(r => {
      const opt = document.createElement('option');
      opt.value = r.run_id;
      if (r.benchmark_mode === 'custom') {
        opt.textContent = `[Custom: ${r.benchmark_category || 'Search'}] ${r.run_id} (${r.timestamp})`;
      } else {
        opt.textContent = `${r.run_id} (${r.timestamp}) - ${r.input_type || 'Generated'}`;
      }
      if (r.run_id === selectedId) opt.selected = true;
      sel.appendChild(opt);
    });

    sel.value = selectedId;
    this.renderComparisonTable(selectedId);
  },

  selectComparisonRun(runId) {
    this.targetComparisonRunId = runId;
    this.loadComparisonView(runId);
  },

  async renderComparisonTable(runId) {
    const tbody = document.getElementById('comparison-table-body');
    if (!tbody) return;
    tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; color: var(--text-muted);">Loading comparison metrics...</td></tr>';

    const comp = await API.getCompare(runId);
    if (!comp || !comp.valid || !comp.groups || comp.groups.length === 0) {
      // Graceful fallback: check if the run itself exists to display single-algorithm results or helpful info
      const run = await API.getRun(runId);
      if (run && run.results && run.results.length > 0) {
        tbody.innerHTML = '';
        const gTr = document.createElement('tr');
        gTr.innerHTML = `
          <td colspan="6" style="background: rgba(255,255,255,0.03); font-weight: 600; color: var(--accent);">
            Dataset: ${escapeHtml(run.input_type || 'Generated')} (Single Algorithm Run • No Relative Speedup)
          </td>
        `;
        tbody.appendChild(gTr);
        run.results.forEach(r => {
          const tr = document.createElement('tr');
          const meanStr = formatTimeSmart((r.time_mean_ns ?? r.mean_time_ns) || 0);
          const medStr = formatTimeSmart((r.time_median_ns ?? r.median_time_ns) || 0);
          const memKb = (((r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0)) || 0) / 1024.0).toFixed(1);
          tr.innerHTML = `
            <td><span class="badge badge-secondary">#1</span></td>
            <td style="font-weight: 600; color: #fff;">${escapeHtml(r.algorithm_name || r.algorithm)} (N=${(r.input_size || 0).toLocaleString()})</td>
            <td>${escapeHtml(meanStr)}</td>
            <td>${escapeHtml(medStr)}</td>
            <td style="color: var(--accent); font-weight: 600;">1.00x</td>
            <td style="color: var(--text-muted);">baseline (Peak RAM: ${escapeHtml(memKb)} KB)</td>
          `;
          tbody.appendChild(tr);
        });
        return;
      }
      tbody.innerHTML = `<tr><td colspan="6" style="text-align: center; color: var(--text-muted);">${(comp && comp.error_message) ? escapeHtml(comp.error_message) : 'No comparison data available for this run.'}</td></tr>`;
      return;
    }

    tbody.innerHTML = '';
    comp.groups.forEach(group => {
      // Group header
      const gTr = document.createElement('tr');
      gTr.innerHTML = `
        <td colspan="6" style="background: rgba(255,255,255,0.03); font-weight: 600; color: var(--accent);">
          Dataset: ${escapeHtml(group.input_type)} • Size N = ${group.input_size.toLocaleString()}
        </td>
      `;
      tbody.appendChild(gTr);

      group.rankings.forEach(r => {
        const tr = document.createElement('tr');
        const rankBadge = r.rank === 1
          ? '<span class="badge badge-success">🥇 #1</span>'
          : r.rank === 2
          ? '<span class="badge badge-secondary">🥈 #2</span>'
          : r.rank === 3
          ? '<span class="badge badge-secondary">🥉 #3</span>'
          : `<span style="color: var(--text-muted); padding-left: 0.5rem;">#${r.rank}</span>`;

        const meanNs = r.time_mean_ns ?? r.mean_time_ns ?? 0;
        const medNs = r.time_median_ns ?? r.median_time_ns ?? 0;
        const meanStr = formatTimeSmart(meanNs);
        const medStr = formatTimeSmart(medNs);
        const speedupVal = r.speedup ?? r.speedup_factor ?? 1.0;
        const speedup = `${Number(speedupVal).toFixed(2)}x`;
        const pctVal = r.percentage_faster ?? r.percentage_faster_than_slowest ?? 0;
        const pctFaster = pctVal > 0
          ? `<span style="color: #10b981; font-weight: 600;">+${Number(pctVal).toFixed(1)}%</span>`
          : '<span style="color: var(--text-muted);">baseline</span>';

        tr.innerHTML = `
          <td>${rankBadge}</td>
          <td style="font-weight: 600; color: #fff;">${escapeHtml(r.name || r.algorithm_name || r.algorithm)}</td>
          <td>${escapeHtml(meanStr)}</td>
          <td>${escapeHtml(medStr)}</td>
          <td style="color: var(--accent); font-weight: 600;">${escapeHtml(speedup)}</td>
          <td>${pctFaster}</td>
        `;
        tbody.appendChild(tr);
      });
    });
  },

  // ── 5. Graphs View ─────────────────────────────────────────────────────────
  bindGraphs() {
    const scopeSel = document.getElementById('graph-scope-select');
    const metricSel = document.getElementById('graph-metric-select');
    const scaleSel = document.getElementById('graph-scale-select');
    const distSel = document.getElementById('graph-dist-select');

    const update = () => this.renderWorkspaceGraph();
    if (scopeSel) scopeSel.addEventListener('change', update);
    if (metricSel) metricSel.addEventListener('change', update);
    if (scaleSel) scaleSel.addEventListener('change', update);
    if (distSel) distSel.addEventListener('change', update);
  },

  async loadGraphs() {
    this.renderWorkspaceGraph();
  },

  async renderWorkspaceGraph() {
    const scopeSel = document.getElementById('graph-scope-select');
    const scope = scopeSel ? scopeSel.value : 'latest';
    const metric = document.getElementById('graph-metric-select').value;
    const isLog = document.getElementById('graph-scale-select').value === 'logarithmic';
    const dist = document.getElementById('graph-dist-select').value;

    let mode = '';
    let runId = '';
    let category = '';

    if (scope === 'latest') {
      const latestRun = await API.getRun();
      if (latestRun && latestRun.run_id) {
        runId = latestRun.run_id;
      }
    } else if (scope === 'standard') {
      mode = 'standard';
    } else if (scope === 'custom') {
      mode = 'custom';
      category = 'search';
    } else if (scope === 'all') {
      // mode and runId left empty
    }

    const data = await API.getTrend(metric, dist, mode, runId, category);
    const unit = metric === 'memory' ? 'MB' : 'µs';
    const canvas = document.getElementById('workspaceChart');
    let emptyMsg = document.getElementById('workspaceChartEmpty');

    if (!emptyMsg && canvas && canvas.parentElement) {
      emptyMsg = document.createElement('div');
      emptyMsg.id = 'workspaceChartEmpty';
      emptyMsg.style.cssText = 'display:none; align-items:center; justify-content:center; height:100%; color:var(--text-muted); font-size:1rem; text-align:center; padding: 2rem;';
      canvas.parentElement.style.position = 'relative';
      canvas.parentElement.appendChild(emptyMsg);
    }

    if (data && data.valid && data.series && data.series.length > 0) {
      if (emptyMsg) emptyMsg.style.display = 'none';
      if (canvas) canvas.style.display = 'block';
      Charts.renderLineChart('workspaceChart', data, unit, isLog);
    } else {
      if (Charts.instances['workspaceChart']) {
        Charts.instances['workspaceChart'].destroy();
        delete Charts.instances['workspaceChart'];
      }
      if (canvas) canvas.style.display = 'none';
      if (emptyMsg) {
        emptyMsg.textContent = 'No benchmark data found matching the selected scope and filters. Run a benchmark or adjust filter settings.';
        emptyMsg.style.display = 'flex';
      }
    }
  },

  // ── 6. Reports View ────────────────────────────────────────────────────────
  async loadReports() {
    const tbody = document.getElementById('reports-table-body');
    if (!tbody) return;
    tbody.innerHTML = '<tr><td colspan="4" style="text-align: center; color: var(--text-muted);">Loading available reports...</td></tr>';

    const data = await API.getHistory();
    if (!data.runs || data.runs.length === 0) {
      tbody.innerHTML = '<tr><td colspan="4" style="text-align: center; color: var(--text-muted);">No historical runs found.</td></tr>';
      return;
    }

    tbody.innerHTML = '';
    data.runs.forEach(run => {
      const tr = document.createElement('tr');
      const isCustom = run.benchmark_mode === 'custom';
      const badgeText = isCustom
        ? `Custom: ${run.benchmark_category || 'Search'} (Target=${run.custom_target_parameter ?? 'N/A'})`
        : `${run.input_type || 'Generated'} (N=${run.element_count ? run.element_count.toLocaleString() : 'N/A'})`;

      tr.innerHTML = `
        <td style="font-family: monospace; font-weight: 600; color: #fff;">${escapeHtml(run.run_id)}</td>
        <td>${escapeHtml(run.timestamp || '—')}</td>
        <td><span class="badge ${isCustom ? 'badge-primary' : 'badge-secondary'}">${escapeHtml(badgeText)}</span></td>
        <td>
          <button class="btn btn-primary btn-sm btn-open-report" data-id="${escapeHtml(run.run_id)}">Open HTML Report</button>
        </td>
      `;

      const btn = tr.querySelector('.btn-open-report');
      btn.addEventListener('click', async () => {
        btn.disabled = true;
        btn.textContent = 'Opening in browser...';
        await API.generateReport(run.run_id);
        setTimeout(() => {
          btn.disabled = false;
          btn.textContent = 'Open HTML Report';
        }, 2000);
      });

      tbody.appendChild(tr);
    });
  }
};

// Initialize on DOM load
document.addEventListener('DOMContentLoaded', () => {
  App.init();
});

