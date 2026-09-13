// Code Performance Analyzer V4 - Main Web Application Controller

const App = {
  activeView: 'dashboard',
  pollTimer: null,

  init() {
    this.bindNavigation();
    this.bindDashboard();
    this.bindBenchmarkForm();
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
          const timeUs = (meanNs / 1000.0).toFixed(2);
          const sp = fastest.speedup ?? fastest.speedup_factor ?? 1.0;
          document.getElementById('dash-fastest-sub').textContent = `${timeUs} µs • ${sp.toFixed(1)}x vs slowest`;
        }

        if (slowest) {
          const sname = slowest.name || slowest.algorithm_name || slowest.algorithm;
          document.getElementById('dash-slowest-name').textContent = sname;
          const meanNs = slowest.time_mean_ns ?? slowest.mean_time_ns ?? 0;
          const timeUs = (meanNs / 1000.0).toFixed(2);
          document.getElementById('dash-slowest-sub').textContent = `${timeUs} µs • baseline (1.0x)`;
        }

        // Render Speedup horizontal bar chart
        const labels = group.rankings.map(r => r.name || r.algorithm_name || r.algorithm).reverse();
        const speedups = group.rankings.map(r => r.speedup ?? r.speedup_factor ?? 1.0).reverse();
        Charts.renderBarChart('dashSpeedupChart', labels, speedups, 'Speedup Factor');
      } else if (run && run.results && run.results.length > 0) {
        // Fallback when only 1 algorithm was benchmarked or homogeneous comparison
        const first = run.results[0];
        const name = first.algorithm_name || first.algorithm;
        const timeUs = (first.time_mean_ns / 1000.0).toFixed(2);
        document.getElementById('dash-fastest-name').textContent = name;
        document.getElementById('dash-fastest-sub').textContent = `${timeUs} µs • Benchmarked`;
        document.getElementById('dash-slowest-name').textContent = name;
        document.getElementById('dash-slowest-sub').textContent = `${timeUs} µs • baseline`;

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

      // 5. Render Dashboard Trend Charts
      const timeTrend = await API.getTrend('time');
      if (timeTrend && timeTrend.valid) {
        Charts.renderLineChart('dashTimeChart', timeTrend, 'µs');
      }

      const memTrend = await API.getTrend('memory');
      if (memTrend && memTrend.valid) {
        Charts.renderLineChart('dashMemoryChart', memTrend, 'MB');
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
    if (affinitySelect && sys.logical_cpus && affinitySelect.options.length <= 1) {
      for (let i = 0; i < sys.logical_cpus; ++i) {
        const opt = document.createElement('option');
        opt.value = i;
        opt.textContent = `Core ${i} (Pinned)`;
        affinitySelect.appendChild(opt);
      }
    }
  },

  // ── 2. Run Benchmark Form ──────────────────────────────────────────────────
  bindBenchmarkForm() {
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
          btnRun.disabled = false;
          btnRun.style.display = 'inline-block';
          btnCancel.disabled = false;
          btnCancel.style.display = 'none';
          btnCancel.textContent = 'Cancel Benchmark';

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
        btnRun.disabled = false;
        btnRun.style.display = 'inline-block';
        btnCancel.disabled = false;
        btnCancel.style.display = 'none';
        btnCancel.textContent = 'Cancel Benchmark';
      } else if (status.state === 'failed') {
        clearInterval(this.pollTimer);
        this.pollTimer = null;
        progTask.textContent = `Benchmark failed: ${status.error_message}`;
        this.updateEngineBadge('Engine Error', '#f43f5e');
        btnRun.disabled = false;
        btnRun.style.display = 'inline-block';
        btnCancel.disabled = false;
        btnCancel.style.display = 'none';
        btnCancel.textContent = 'Cancel Benchmark';
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

    // Subtitle with Run ID, timestamp, and input type
    const sub = document.getElementById('bench-res-subtitle');
    if (sub) {
      const inType = run.input_type || (run.input ? run.input.type : '') || 'Generated';
      sub.textContent = `Run ID: ${run.run_id} • Date: ${run.timestamp || 'Latest'} • Input: ${inType}`;
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
      const timeUs = (meanNs / 1000.0).toFixed(2);
      const sp = fastestAlg.speedup ?? fastestAlg.speedup_factor ?? 1.0;
      document.getElementById('bench-fastest-sub').textContent = `${timeUs} µs • ${Number(sp).toFixed(2)}x vs slowest`;
    }

    if (slowestAlg) {
      const sname = slowestAlg.name || slowestAlg.algorithm_name || slowestAlg.algorithm;
      document.getElementById('bench-slowest-name').textContent = sname;
      const meanNs = slowestAlg.time_mean_ns ?? slowestAlg.mean_time_ns ?? 0;
      const timeUs = (meanNs / 1000.0).toFixed(2);
      document.getElementById('bench-slowest-sub').textContent = `${timeUs} µs • baseline (1.0x)`;
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
    document.getElementById('bench-scope-val').textContent = `${uniqueAlgs.size} Algorithms`;
    document.getElementById('bench-scope-sub').textContent = `${run.results.length} runs across N = ${Array.from(uniqueSizes).join(', ')}`;

    // Build Time & Memory Charts for this specific run
    const timeSeriesMap = {};
    const memSeriesMap = {};
    run.results.forEach(r => {
      const name = r.algorithm_name || r.name || r.algorithm;
      if (!timeSeriesMap[name]) {
        timeSeriesMap[name] = { algorithm: r.algorithm, name: name, points: [] };
      }
      if (!memSeriesMap[name]) {
        memSeriesMap[name] = { algorithm: r.algorithm, name: name, points: [] };
      }
      const meanUs = (r.time_mean_ns ?? r.mean_time_ns ?? 0) / 1000.0;
      timeSeriesMap[name].points.push({ x: r.input_size, y: meanUs });

      const peakBytes = r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0) ?? 0;
      const peakVal = peakBytes / (1024.0 * 1024.0);
      memSeriesMap[name].points.push({ x: r.input_size, y: peakVal });
    });

    const timeChartData = {
      valid: true,
      title: 'Execution Time vs Input Size (N)',
      x_label: 'Input Size (N)',
      y_label: 'Mean Time (µs)',
      series: Object.values(timeSeriesMap).map(s => {
        s.points.sort((a, b) => a.x - b.x);
        return s;
      })
    };
    Charts.renderLineChart('benchTimeChart', timeChartData, 'µs');

    const memChartData = {
      valid: true,
      title: 'Peak Memory vs Input Size (N)',
      x_label: 'Input Size (N)',
      y_label: 'MB Footprint',
      series: Object.values(memSeriesMap).map(s => {
        s.points.sort((a, b) => a.x - b.x);
        return s;
      })
    };
    Charts.renderLineChart('benchMemoryChart', memChartData, 'MB');

    // Detailed Table
    const tbody = document.getElementById('bench-table-body');
    const tableCount = document.getElementById('bench-table-count');
    if (tableCount) tableCount.textContent = `${run.results.length} total measurements`;

    if (tbody) {
      tbody.innerHTML = '';
      run.results.forEach(r => {
        const tr = document.createElement('tr');
        const name = r.algorithm_name || r.name || r.algorithm;
        const medUs = ((r.time_median_ns ?? r.median_time_ns ?? 0) / 1000.0).toFixed(2);
        const meanUs = ((r.time_mean_ns ?? r.mean_time_ns ?? 0) / 1000.0).toFixed(2);
        const minUs = ((r.time_min_ns ?? 0) / 1000.0).toFixed(2);
        const maxUs = ((r.time_max_ns ?? 0) / 1000.0).toFixed(2);
        const peakBytes = r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0) ?? 0;
        const peakMb = (peakBytes / (1024.0 * 1024.0)).toFixed(2);

        const rk = rankingMap[name] || rankingMap[r.algorithm];
        const spVal = rk ? (rk.speedup ?? rk.speedup_factor ?? 1.0) : 1.0;
        const spText = `${Number(spVal).toFixed(2)}x`;

        tr.innerHTML = `
          <td style="font-weight: 600; color: #fff;">${name}</td>
          <td>${(r.input_size || 0).toLocaleString()}</td>
          <td><span class="badge badge-neutral">${r.input_type || 'Random'}</span></td>
          <td>${medUs} µs</td>
          <td>${meanUs} µs</td>
          <td style="color: var(--text-muted); font-size: 0.8rem;">${minUs} / ${maxUs} µs</td>
          <td>${peakMb} MB</td>
          <td style="color: var(--accent); font-weight: 600;">${spText}</td>
          <td><span class="badge badge-success">PASS ✓</span></td>
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

      tr.innerHTML = `
        <td style="font-family: monospace; font-weight: 600; color: #fff;">${run.run_id}</td>
        <td>${run.timestamp || '—'}</td>
        <td><span class="badge badge-secondary">${run.input_type || 'Generated'}</span></td>
        <td>${run.element_count ? run.element_count.toLocaleString() : 'N/A'}</td>
        <td style="color: var(--text-muted); font-size: 0.85rem;">${algsStr}</td>
        <td>
          <div style="display: flex; gap: 0.5rem;">
            <button class="btn btn-secondary btn-sm btn-hist-compare" data-id="${run.run_id}">Compare</button>
            <button class="btn btn-secondary btn-sm btn-hist-report" data-id="${run.run_id}">HTML Report</button>
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
      opt.textContent = `${r.run_id} (${r.timestamp}) - ${r.input_type}`;
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
            Dataset: ${run.input_type || 'Generated'} (Single Algorithm Run • No Relative Speedup)
          </td>
        `;
        tbody.appendChild(gTr);
        run.results.forEach(r => {
          const tr = document.createElement('tr');
          const meanUs = (((r.time_mean_ns ?? r.mean_time_ns) || 0) / 1000.0).toFixed(2);
          const medUs = (((r.time_median_ns ?? r.median_time_ns) || 0) / 1000.0).toFixed(2);
          const memKb = (((r.memory_peak_increase_bytes ?? (r.memory ? r.memory.peak_private_increase_bytes : 0)) || 0) / 1024.0).toFixed(1);
          tr.innerHTML = `
            <td><span class="badge badge-secondary">#1</span></td>
            <td style="font-weight: 600; color: #fff;">${r.algorithm_name || r.algorithm} (N=${(r.input_size || 0).toLocaleString()})</td>
            <td>${meanUs} µs</td>
            <td>${medUs} µs</td>
            <td style="color: var(--accent); font-weight: 600;">1.00x</td>
            <td style="color: var(--text-muted);">baseline (Peak RAM: ${memKb} KB)</td>
          `;
          tbody.appendChild(tr);
        });
        return;
      }
      tbody.innerHTML = `<tr><td colspan="6" style="text-align: center; color: var(--text-muted);">${(comp && comp.error_message) ? comp.error_message : 'No comparison data available for this run.'}</td></tr>`;
      return;
    }

    tbody.innerHTML = '';
    comp.groups.forEach(group => {
      // Group header
      const gTr = document.createElement('tr');
      gTr.innerHTML = `
        <td colspan="6" style="background: rgba(255,255,255,0.03); font-weight: 600; color: var(--accent);">
          Dataset: ${group.input_type} • Size N = ${group.input_size.toLocaleString()}
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
        const meanUs = (meanNs / 1000.0).toFixed(2);
        const medUs = (medNs / 1000.0).toFixed(2);
        const speedupVal = r.speedup ?? r.speedup_factor ?? 1.0;
        const speedup = `${Number(speedupVal).toFixed(2)}x`;
        const pctVal = r.percentage_faster ?? r.percentage_faster_than_slowest ?? 0;
        const pctFaster = pctVal > 0
          ? `<span style="color: #10b981; font-weight: 600;">+${Number(pctVal).toFixed(1)}%</span>`
          : '<span style="color: var(--text-muted);">baseline</span>';

        tr.innerHTML = `
          <td>${rankBadge}</td>
          <td style="font-weight: 600; color: #fff;">${r.name || r.algorithm_name || r.algorithm}</td>
          <td>${meanUs} µs</td>
          <td>${medUs} µs</td>
          <td style="color: var(--accent); font-weight: 600;">${speedup}</td>
          <td>${pctFaster}</td>
        `;
        tbody.appendChild(tr);
      });
    });
  },

  // ── 5. Graphs View ─────────────────────────────────────────────────────────
  bindGraphs() {
    const metricSel = document.getElementById('graph-metric-select');
    const scaleSel = document.getElementById('graph-scale-select');
    const distSel = document.getElementById('graph-dist-select');

    const update = () => this.renderWorkspaceGraph();
    if (metricSel) metricSel.addEventListener('change', update);
    if (scaleSel) scaleSel.addEventListener('change', update);
    if (distSel) distSel.addEventListener('change', update);
  },

  async loadGraphs() {
    this.renderWorkspaceGraph();
  },

  async renderWorkspaceGraph() {
    const metric = document.getElementById('graph-metric-select').value;
    const isLog = document.getElementById('graph-scale-select').value === 'logarithmic';
    const dist = document.getElementById('graph-dist-select').value;

    const data = await API.getTrend(metric, dist);
    const unit = metric === 'memory' ? 'MB' : 'µs';
    if (data && data.valid) {
      Charts.renderLineChart('workspaceChart', data, unit, isLog);
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
      tr.innerHTML = `
        <td style="font-family: monospace; font-weight: 600; color: #fff;">${run.run_id}</td>
        <td>${run.timestamp || '—'}</td>
        <td><span class="badge badge-secondary">${run.input_type || 'Generated'} (N=${run.element_count ? run.element_count.toLocaleString() : 'N/A'})</span></td>
        <td>
          <button class="btn btn-primary btn-sm btn-open-report" data-id="${run.run_id}">Open HTML Report</button>
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

