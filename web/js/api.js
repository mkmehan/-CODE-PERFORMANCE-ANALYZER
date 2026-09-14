// API communication layer for Code Performance Analyzer V4
const API = {
  baseUrl: window.location.origin,

  async getStatus() {
    try {
      const res = await fetch(`${this.baseUrl}/api/status`);
      return await res.json();
    } catch (err) {
      console.error('API.getStatus error:', err);
      return { state: 'failed', error_message: 'Connection failed' };
    }
  },

  async getHistory() {
    try {
      const res = await fetch(`${this.baseUrl}/api/history`);
      return await res.json();
    } catch (err) {
      console.error('API.getHistory error:', err);
      return { runs: [] };
    }
  },

  async getRun(id = '') {
    try {
      const url = id ? `${this.baseUrl}/api/run?id=${encodeURIComponent(id)}` : `${this.baseUrl}/api/run`;
      const res = await fetch(url);
      return await res.json();
    } catch (err) {
      console.error('API.getRun error:', err);
      return { error: 'Failed to fetch run data' };
    }
  },

  async getCompare(id = '') {
    try {
      const url = id ? `${this.baseUrl}/api/compare?id=${encodeURIComponent(id)}` : `${this.baseUrl}/api/compare`;
      const res = await fetch(url);
      return await res.json();
    } catch (err) {
      console.error('API.getCompare error:', err);
      return { valid: false, error_message: 'Failed to fetch comparison' };
    }
  },

  async getRegression(id = '') {
    try {
      const url = id ? `${this.baseUrl}/api/regression?id=${encodeURIComponent(id)}` : `${this.baseUrl}/api/regression`;
      const res = await fetch(url);
      return await res.json();
    } catch (err) {
      console.error('API.getRegression error:', err);
      return { valid: false, error: 'Failed to fetch regression' };
    }
  },

  async getTrend(metric = 'time', dist = '') {
    try {
      let url = `${this.baseUrl}/api/trend?metric=${encodeURIComponent(metric)}`;
      if (dist) url += `&dist=${encodeURIComponent(dist)}`;
      const res = await fetch(url);
      return await res.json();
    } catch (err) {
      console.error('API.getTrend error:', err);
      return { valid: false, series: [] };
    }
  },

  async validateFile(filePath) {
    try {
      const res = await fetch(`${this.baseUrl}/api/validate-file`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ file_path: filePath })
      });
      return await res.json();
    } catch (err) {
      console.error('API.validateFile error:', err);
      return { valid: false, error_message: err.message };
    }
  },

  async uploadDataset(filename, content) {
    try {
      const res = await fetch(`${this.baseUrl}/api/upload-dataset`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ filename, content })
      });
      return await res.json();
    } catch (err) {
      console.error('API.uploadDataset error:', err);
      return { valid: false, error_message: err.message };
    }
  },

  async startBenchmark(config) {
    try {
      const res = await fetch(`${this.baseUrl}/api/benchmark`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      });
      return await res.json();
    } catch (err) {
      console.error('API.startBenchmark error:', err);
      return { status: 'failed', error_message: err.message };
    }
  },

  async cancelBenchmark() {
    try {
      const res = await fetch(`${this.baseUrl}/api/benchmark/cancel`, { method: 'POST' });
      return await res.json();
    } catch (err) {
      console.error('API.cancelBenchmark error:', err);
      return { status: 'failed' };
    }
  },

  async generateReport(id = '') {
    try {
      const url = id ? `${this.baseUrl}/api/report?id=${encodeURIComponent(id)}` : `${this.baseUrl}/api/report`;
      const res = await fetch(url, { method: 'POST' });
      return await res.json();
    } catch (err) {
      console.error('API.generateReport error:', err);
      return { success: false };
    }
  },

  async detectInterface(sourceCode = '', filePath = '', category = 'search') {
    try {
      const res = await fetch(`${this.baseUrl}/api/custom-benchmark/detect-interface`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source_code: sourceCode, file_path: filePath, category })
      });
      return await res.json();
    } catch (err) {
      console.error('API.detectInterface error:', err);
      return { recognized: false, diagnostic_message: err.message };
    }
  },

  async uploadCustomAlgorithm(filename, content) {
    try {
      const res = await fetch(`${this.baseUrl}/api/custom-benchmark/upload-algorithm`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ filename, content })
      });
      return await res.json();
    } catch (err) {
      console.error('API.uploadCustomAlgorithm error:', err);
      return { recognized: false, diagnostic_message: err.message };
    }
  },

  async getCustomSamples() {
    try {
      const res = await fetch(`${this.baseUrl}/api/custom-benchmark/samples`);
      return await res.json();
    } catch (err) {
      console.error('API.getCustomSamples error:', err);
      return null;
    }
  },

  async detectTarget(datasetPath, target) {
    try {
      const res = await fetch(`${this.baseUrl}/api/custom-benchmark/detect-target`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dataset_path: datasetPath, target: parseInt(target, 10) })
      });
      return await res.json();
    } catch (err) {
      console.error('API.detectTarget error:', err);
      return { valid: false, error_message: err.message };
    }
  },

  async startCustomBenchmark(config) {
    try {
      const res = await fetch(`${this.baseUrl}/api/custom-benchmark/start`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      });
      return await res.json();
    } catch (err) {
      console.error('API.startCustomBenchmark error:', err);
      return { status: 'failed', error_message: err.message };
    }
  }
};

