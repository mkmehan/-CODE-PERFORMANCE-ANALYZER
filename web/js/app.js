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
    } catch (err) {
      console.error('Error loading dashboard:', err);
    }
  },

  // ── 2. Run Benchmark Form ──────────────────────────────────────────────────
  bindBenchmarkForm() {
    const inputTypeSelect = document.getElementById('cfg-input-type');
    const fileGroup = document.getElementById('cfg-file-group');
    const btnValidate = document.getElementById('btn-validate-file');
    const filePathInput = document.getElementById('cfg-file-path');
    const validationMsg = document.getElementById('file-validation-msg');

    if (inputTypeSelect) {
      inputTypeSelect.addEventListener('change', () => {
        if (inputTypeSelect.value === 'CustomFile') {
          fileGroup.style.display = 'block';
        } else {
          fileGroup.style.display = 'none';
          validationMsg.textContent = '';
        }
      });
    }

    if (btnValidate && filePathInput) {
      btnValidate.addEventListener('click', async () => {
        const path = filePathInput.value.trim();
        if (!path) {
          validationMsg.style.color = '#f43f5e';
          validationMsg.textContent = 'Please enter a valid file path.';
          return;
        }
        btnValidate.disabled = true;
        btnValidate.textContent = 'Checking...';
        const res = await API.validateFile(path);
        btnValidate.disabled = false;
        btnValidate.textContent = 'Validate File';

        if (res.valid) {
          validationMsg.style.color = '#10b981';
          validationMsg.textContent = `✓ Valid dataset: ${res.element_count.toLocaleString()} elements, ${res.order}, min: ${res.min_value}, max: ${res.max_value}`;
        } else {
          validationMsg.style.color = '#f43f5e';
          validationMsg.textContent = `✗ Error: ${res.error_message || 'File could not be parsed'}`;
        }
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

        const payload = {
          input_type: inputType,
          file_path: filePath,
          algorithms: algs,
          sizes: sizes,
          iterations: iterations,
          warmup: warmup,
          memory: memory
        };

        btnRun.disabled = true;
        btnRun.style.display = 'none';
        btnCancel.style.display = 'inline-block';
        progBox.classList.add('active');

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

        setTimeout(() => {
          btnRun.disabled = false;
          btnRun.style.display = 'inline-block';
          btnCancel.disabled = false;
          btnCancel.style.display = 'none';
          btnCancel.textContent = 'Cancel Benchmark';
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

