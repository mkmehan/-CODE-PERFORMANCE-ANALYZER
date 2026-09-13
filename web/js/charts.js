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

    // Extract sorted unique X points
    const xSet = new Set();
    chartData.series.forEach(s => {
      s.points.forEach(p => xSet.add(p.x));
    });
    const xLabels = Array.from(xSet).sort((a, b) => a - b);

    const datasets = chartData.series.map((s, idx) => {
      const color = this.getColor(idx);
      const pointMap = {};
      s.points.forEach(p => { pointMap[p.x] = p.y; });

      return {
        label: s.name || s.algorithm,
        borderColor: color,
        backgroundColor: this.getColor(idx, 0.1),
        borderWidth: 2,
        tension: 0.25,
        pointRadius: 4,
        pointHoverRadius: 6,
        data: xLabels.map(x => {
          const val = pointMap[x];
          if (val === undefined || val === null) return null;
          if (yLogScale && val <= 0) return null;
          return val;
        })
      };
    });

    this.instances[canvasId] = new Chart(ctx.getContext('2d'), {
      type: 'line',
      data: {
        labels: xLabels.map(x => x.toLocaleString()),
        datasets: datasets
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: { mode: 'index', intersect: false },
        scales: {
          x: {
            title: { display: true, text: chartData.x_label || 'Input Size (N)', color: '#9ca3af' },
            ...this.darkTheme
          },
          y: {
            type: yLogScale ? 'logarithmic' : 'linear',
            min: yLogScale ? 0.1 : undefined,
            title: { display: true, text: chartData.y_label || `Value (${unit})`, color: '#9ca3af' },
            ...this.darkTheme
          }
        },
        plugins: {
          legend: { labels: { color: '#f3f4f6', boxWidth: 12 } },
          tooltip: {
            callbacks: {
              label: (c) => `${c.dataset.label}: ${c.parsed.y !== null ? c.parsed.y.toFixed(2) + ' ' + unit : 'N/A'}`
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

