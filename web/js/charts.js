// Chart.js manager for Code Performance Analyzer V4
const Charts = {
  palette: [
    '#06b6d4', // Cyan
    '#10b981', // Emerald
    '#8b5cf6', // Purple
    '#f59e0b', // Amber
    '#f43f5e', // Rose
    '#3b82f6', // Blue
    '#ec4899', // Pink
    '#14b8a6', // Teal
    '#a855f7', // Violet
    '#eab308'  // Yellow
  ],

  darkTheme: {
    color: '#9ca3af',
    grid: { color: 'rgba(255, 255, 255, 0.06)' },
    border: { color: 'rgba(255, 255, 255, 0.1)' }
  },

  instances: {},

  getColor(index, alpha = 1.0) {
    const hex = this.palette[index % this.palette.length];
    if (alpha >= 0.99) return hex;
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
  },

  renderLineChart(canvasId, chartData, unit = 'µs', yLogScale = false) {
    const ctx = document.getElementById(canvasId);
    if (!ctx) return;

    if (this.instances[canvasId]) {
      this.instances[canvasId].destroy();
    }

    if (!chartData || !chartData.series || chartData.series.length === 0) {
      return;
    }

    // Check for valid positive values if log scale requested
    let effectiveLog = yLogScale;
    if (yLogScale) {
      let hasZeroOrNeg = false;
      chartData.series.forEach(s => {
        (s.points || []).forEach(p => {
          if (p.y !== undefined && p.y !== null && Number(p.y) <= 0) {
            hasZeroOrNeg = true;
          }
        });
      });
      if (hasZeroOrNeg) {
        effectiveLog = false; // Gracefully fall back to linear scale
      }
    }

    const datasets = chartData.series.map((s, idx) => {
      const color = this.getColor(idx);
      const pts = (s.points || [])
        .filter(p => p && p.x !== undefined && p.y !== undefined && p.y !== null)
        .map(p => ({ x: Number(p.x), y: Number(p.y) }))
        .filter(p => !effectiveLog || p.y > 0)
        .sort((a, b) => a.x - b.x);

      return {
        label: s.name || s.algorithm_name || s.algorithm || `Series ${idx + 1}`,
        borderColor: color,
        backgroundColor: this.getColor(idx, 0.12),
        borderWidth: 2.2,
        tension: 0.1,
        pointRadius: 4,
        pointHoverRadius: 6,
        spanGaps: false, // Default false: Never falsely connect missing measurements
        data: pts
      };
    });

    this.instances[canvasId] = new Chart(ctx.getContext('2d'), {
      type: 'line',
      data: {
        datasets: datasets
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: { mode: 'nearest', intersect: false, axis: 'x' },
        scales: {
          x: {
            type: 'linear',
            title: { display: true, text: chartData.x_label || 'Input Size (N)', color: '#9ca3af' },
            ticks: {
              color: '#9ca3af',
              callback: (val) => Number(val).toLocaleString()
            },
            ...this.darkTheme
          },
          y: {
            type: effectiveLog ? 'logarithmic' : 'linear',
            min: effectiveLog ? 0.01 : undefined,
            title: { display: true, text: chartData.y_label || `Value (${unit})`, color: '#9ca3af' },
            ticks: {
              color: '#9ca3af',
              callback: (val) => {
                if (typeof val === 'number') {
                  if (val >= 1000000) return (val / 1000000).toFixed(1) + 'M';
                  if (val >= 1000) return (val / 1000).toFixed(1) + 'k';
                  if (val > 0 && val < 0.1) return val.toFixed(3);
                  return val.toLocaleString();
                }
                return val;
              }
            },
            ...this.darkTheme
          }
        },
        plugins: {
          legend: { labels: { color: '#f3f4f6', boxWidth: 12, padding: 12 } },
          tooltip: {
            callbacks: {
              title: (items) => {
                if (!items || items.length === 0) return '';
                return `N = ${Number(items[0].parsed.x).toLocaleString()}`;
              },
              label: (c) => `${c.dataset.label}: ${c.parsed.y !== null && c.parsed.y !== undefined ? Number(c.parsed.y).toFixed(3) + ' ' + unit : 'N/A'}`
            }
          }
        }
      }
    });
  },

  renderBarChart(canvasId, labels, values, title = 'Speedup Factor') {
    const ctx = document.getElementById(canvasId);
    if (!ctx) return;

    if (this.instances[canvasId]) {
      this.instances[canvasId].destroy();
    }

    this.instances[canvasId] = new Chart(ctx.getContext('2d'), {
      type: 'bar',
      data: {
        labels: labels,
        datasets: [{
          label: title,
          data: values,
          backgroundColor: labels.map((_, i) => this.getColor(i, 0.8)),
          borderColor: labels.map((_, i) => this.getColor(i)),
          borderWidth: 1,
          borderRadius: 6
        }]
      },
      options: {
        indexAxis: 'y',
        responsive: true,
        maintainAspectRatio: false,
        scales: {
          x: {
            title: { display: true, text: 'Speedup vs Slowest (1.0x baseline)', color: '#9ca3af' },
            ...this.darkTheme
          },
          y: { ...this.darkTheme }
        },
        plugins: {
          legend: { display: false },
          tooltip: {
            callbacks: {
              label: (c) => `Speedup: ${c.parsed.x.toFixed(2)}x faster`
            }
          }
        }
      }
    });
  }
};

