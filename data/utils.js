// utils.js - Shared utilities for ALL pages
class StatusManager {
    static showStatus(message, type = 'info') {
        const status = document.getElementById('status');
        if (!status) {
            console.warn('Status element not found');
            return;
        }
        
        status.classList.remove('hiding');
        status.textContent = message;
        status.className = 'status-message ' + type;
        status.style.display = 'block';
        
        setTimeout(() => {
            status.classList.add('hiding');
            setTimeout(() => {
                status.style.display = 'none';
                status.classList.remove('hiding');
            }, 300);
        }, 5000);
    }
}

class ApiClient {
    static async request(url, options = {}) {
        try {
            const response = await fetch(url, options);
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return await response.json();
        } catch (error) {
            StatusManager.showStatus(`Error: ${error.message}`, 'error');
            throw error;
        }
    }

    static async get(url) {
        return this.request(url);
    }

    static async post(url, data) {
        return this.request(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
    }

    static async postForm(url, formData) {
        try {
            const response = await fetch(url, {
                method: 'POST',
                body: formData
            });
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return await response.text();
        } catch (error) {
            StatusManager.showStatus(`Error: ${error.message}`, 'error');
            throw error;
        }
    }
}

class FormHelper {
    static getElement(id) {
        return document.getElementById(id);
    }

    static getValue(id) {
        const element = this.getElement(id);
        return element ? element.value : '';
    }

    static setValue(id, value) {
        const element = this.getElement(id);
        if (element) element.value = value || '';
    }

    static clearForm(ids) {
        ids.forEach(id => this.setValue(id, ''));
    }

    static validateRequired(ids) {
        for (const id of ids) {
            if (!this.getValue(id)) {
                StatusManager.showStatus(`Please fill in ${id}`, 'error');
                this.getElement(id)?.focus();
                return false;
            }
        }
        return true;
    }
}

// Backward compatibility - keep existing function names working
function showStatus(message, type = 'info') {
    StatusManager.showStatus(message, type);
}