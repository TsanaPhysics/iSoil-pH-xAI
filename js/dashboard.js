/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: js/dashboard.js
 * Description: Real-time UI updating engine, Chart.js integration, and API polling
 */

let phChart = null;
let isPolling = true;
let pollTimer = null;
let lastKnownId = 0;
let currentSession = '';

const POLL_INTERVAL = 2000; // ทุก 2 วินาที

document.addEventListener('DOMContentLoaded', () => {
    initChart();
    fetchDashboardData();
    startPolling();

    // Event Listeners
    document.getElementById('btnTogglePoll').addEventListener('click', togglePolling);
    document.getElementById('btnSimulate').addEventListener('click', simulateSample);

    const sessionSelect = document.getElementById('sessionSelect');
    if (sessionSelect) {
        sessionSelect.addEventListener('change', (e) => {
            currentSession = e.target.value;
            updateExportLink();
            fetchDashboardData();
        });
    }

    const btnNewSession = document.getElementById('btnNewSession');
    if (btnNewSession) {
        btnNewSession.addEventListener('click', async () => {
            if (!confirm('ต้องการเริ่มการทดลองและสร้างไฟล์บันทึกเซสชันใหม่ใช่หรือไม่?')) return;
            try {
                btnNewSession.disabled = true;
                btnNewSession.innerText = '⏳ กำลังเริ่ม...';
                const res = await fetch('api/new_session.php');
                const result = await res.json();
                if (result.success) {
                    currentSession = result.session_id;
                    updateExportLink();
                    await fetchDashboardData();
                    alert(`เริ่มเซสชันใหม่สำเร็จ: ${result.session_id}\n(ระบบจะเริ่มบันทึกไฟล์ใหม่บน MicroSD Card และฐานข้อมูลทันที)`);
                } else {
                    alert('เกิดข้อผิดพลาด: ' + (result.error || 'ไม่สามารถเริ่มเซสชันใหม่ได้'));
                }
            } catch (err) {
                alert('เกิดข้อผิดพลาดในการเชื่อมต่อ: ' + err.message);
            } finally {
                btnNewSession.disabled = false;
                btnNewSession.innerText = '➕ เริ่มใหม่ (New)';
            }
        });
    }
});

function updateExportLink() {
    const btn = document.getElementById('btnExportCsv');
    if (!btn) return;
    if (currentSession && currentSession !== 'all') {
        btn.href = `api/export_csv.php?session_id=${encodeURIComponent(currentSession)}`;
    } else {
        btn.href = 'api/export_csv.php';
    }
}

// ----------------- Chart.js Initialization -----------------
function initChart() {
    const ctx = document.getElementById('phTrendChart').getContext('2d');
    
    // Gradient สำหรับเส้น AI
    const gradientAI = ctx.createLinearGradient(0, 0, 0, 300);
    gradientAI.addColorStop(0, 'rgba(0, 230, 130, 0.35)');
    gradientAI.addColorStop(1, 'rgba(0, 230, 130, 0.0)');

    phChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'AI-Compensated pH (ANN)',
                    data: [],
                    borderColor: '#00e682',
                    backgroundColor: gradientAI,
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.35,
                    pointRadius: 3,
                    pointHoverRadius: 6,
                    pointBackgroundColor: '#00e682'
                },
                {
                    label: 'Traditional (Nernst)',
                    data: [],
                    borderColor: '#ff9f43',
                    borderWidth: 2,
                    borderDash: [5, 4],
                    fill: false,
                    tension: 0.35,
                    pointRadius: 2,
                    pointHoverRadius: 5,
                    pointBackgroundColor: '#ff9f43'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: {
                    labels: {
                        color: '#8e9bb0',
                        font: { family: 'Plus Jakarta Sans', size: 12, weight: '600' },
                        boxWidth: 14
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(15, 23, 42, 0.95)',
                    titleColor: '#f0f4fc',
                    bodyColor: '#8e9bb0',
                    borderColor: 'rgba(0, 230, 130, 0.3)',
                    borderWidth: 1,
                    padding: 12,
                    cornerRadius: 8
                }
            },
            scales: {
                x: {
                    grid: { color: 'rgba(60, 80, 115, 0.15)' },
                    ticks: { color: '#5c6880', font: { family: 'JetBrains Mono', size: 10 } }
                },
                y: {
                    min: 3.0,
                    max: 11.0,
                    grid: { color: 'rgba(60, 80, 115, 0.2)' },
                    ticks: {
                        color: '#8e9bb0',
                        font: { family: 'JetBrains Mono', size: 11 },
                        stepSize: 1.0
                    }
                }
            }
        }
    });
}

// ----------------- Data Fetching & UI Update -----------------
async function fetchDashboardData() {
    try {
        let url = 'api/get_data.php?limit=30';
        if (currentSession) {
            url += `&session_id=${encodeURIComponent(currentSession)}`;
        }
        const response = await fetch(url);
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        const result = await response.json();

        if (result.success) {
            updateSessionDropdown(result.sessions, result.active_session);
            updateKPIs(result.latest, result.stats, result.active_session);
            updateSoilBanner(result.latest);
            updateChartData(result.history);
            updateTable(result.history);
            setOnlineStatus(true);
        }
    } catch (err) {
        console.warn('[Dashboard] Fetch error:', err);
        setOnlineStatus(false);
    }
}

function updateSessionDropdown(sessions, activeSession) {
    const sel = document.getElementById('sessionSelect');
    if (!sel || !sessions) return;

    const previousValue = sel.value;
    const expectedOptionsCount = sessions.length + 2; // default + all

    if (sel.options.length !== expectedOptionsCount) {
        let html = '<option value="">📁 เซสชันล่าสุด (Latest)</option>';
        sessions.forEach(s => {
            const sid = s.session_id;
            const cnt = s.total_records;
            html += `<option value="${sid}">🗂️ ${sid} (${cnt} ค่า)</option>`;
        });
        html += '<option value="all">📂 ข้อมูลทุกเซสชัน (All Records)</option>';
        sel.innerHTML = html;
        if (previousValue) sel.value = previousValue;
    }

    const sessBadge = document.getElementById('kpiActiveSession');
    if (sessBadge) {
        sessBadge.textContent = activeSession || 'EXP_001';
    }
}

function updateKPIs(latest, stats, activeSession) {
    if (!latest) return;

    // AI Predicted pH
    const aiEl = document.getElementById('kpiAIPH');
    if (aiEl && latest.ph_ai !== undefined) aiEl.textContent = Number(latest.ph_ai).toFixed(2);

    // Traditional Nernst pH
    const tradEl = document.getElementById('kpiTradPH');
    if (tradEl && latest.ph_traditional !== undefined) tradEl.textContent = Number(latest.ph_traditional).toFixed(2);

    // Delta Compensation Tag
    const deltaEl = document.getElementById('kpiDeltaTag');
    if (deltaEl && latest.delta_error !== undefined) {
        const delta = Number(latest.delta_error);
        deltaEl.textContent = (delta >= 0 ? '+' : '') + delta.toFixed(3) + ' pH';
    }

    // Cell Potential & Temperature
    const voltEl = document.getElementById('kpiVoltage');
    if (voltEl && latest.voltage !== undefined) voltEl.textContent = Number(latest.voltage).toFixed(3) + ' V';
    
    const tempEl = document.getElementById('kpiTemp');
    if (tempEl && latest.temp_c !== undefined) tempEl.textContent = Number(latest.temp_c).toFixed(1) + ' °C';

    const tempModeEl = document.getElementById('kpiTempMode');
    if (tempModeEl) {
        const tMode = latest.temp_mode || 'MTC';
        tempModeEl.textContent = tMode === 'ATC' ? '[ATC 3-in-1]' : '[MTC Manual]';
        tempModeEl.style.background = tMode === 'ATC' ? 'rgba(0,255,140,0.15)' : 'rgba(56,189,248,0.15)';
        tempModeEl.style.color = tMode === 'ATC' ? '#00ff8c' : '#38bdf8';
    }

    // AI Confidence & RMSE
    const confVal = latest.confidence !== undefined ? Number(latest.confidence).toFixed(1) : '98.6';
    const confEl = document.getElementById('kpiConfidence');
    if (confEl) confEl.textContent = confVal + '%';

    const rmseEl = document.getElementById('kpiRMSE');
    if (rmseEl) {
        const modelName = latest.model_name || '';
        let rmseVal = '±0.05 pH';
        if (modelName.includes('LOAM')) rmseVal = '±0.06 pH';
        else if (modelName.includes('CLAY')) rmseVal = '±0.07 pH';
        else if (modelName.includes('UNIV')) rmseVal = '±0.04 pH';
        rmseEl.textContent = rmseVal;
    }

    const confBadge = document.getElementById('systemConfidenceBadge');
    if (confBadge) {
        confBadge.innerHTML = `★ CONF: ${confVal}%`;
    }

    // Total Count & Buffer
    const rowsEl = document.getElementById('kpiTotalRows');
    if (rowsEl && stats) rowsEl.textContent = stats.total_count;
    
    const bufEl = document.getElementById('kpiTargetBuf');
    if (bufEl) bufEl.textContent = latest.target_buffer || 'FIELD';
    
    const optEl = document.getElementById('kpiOptimalPercent');
    if (optEl && stats) optEl.textContent = stats.optimal_percentage + '%';

    // Active Session Badge
    const sessBadge = document.getElementById('kpiActiveSession');
    if (sessBadge) {
        sessBadge.textContent = activeSession || latest.session_id || 'EXP_005';
    }

    // Active Model & Mode
    const modelEl = document.getElementById('kpiActiveModel');
    if (modelEl) modelEl.textContent = latest.model_name || 'ANN DURIAN';
    const modeEl = document.getElementById('kpiActiveMode');
    if (modeEl) modeEl.textContent = latest.mode || 'FIELD_RUN';

    // Header Badges
    const modeBadge = document.getElementById('systemModeBadge');
    if (modeBadge && latest.mode) {
        if (latest.mode === 'CALIBRATE') {
            modeBadge.innerHTML = '🧪 CALIBRATE';
            modeBadge.style.color = '#38bdf8';
            modeBadge.style.borderColor = 'rgba(56, 189, 248, 0.5)';
            modeBadge.style.background = 'rgba(56, 189, 248, 0.15)';
        } else if (latest.mode === 'AI_LEARN') {
            modeBadge.innerHTML = '🧬 AI LEARN';
            modeBadge.style.color = '#c084fc';
            modeBadge.style.borderColor = 'rgba(192, 132, 252, 0.5)';
            modeBadge.style.background = 'rgba(192, 132, 252, 0.15)';
        } else {
            modeBadge.innerHTML = '🚀 FIELD RUN';
            modeBadge.style.color = '#00e682';
            modeBadge.style.borderColor = 'rgba(0, 230, 130, 0.5)';
            modeBadge.style.background = 'rgba(0, 230, 130, 0.15)';
        }
    }

    const modelBadge = document.getElementById('systemModelBadge');
    if (modelBadge && latest.model_name) {
        modelBadge.innerHTML = `🌳 ${latest.model_name}`;
    }

    // Sample Date & Time
    const dtEl = document.getElementById('kpiSampleDateTime');
    if (dtEl && latest.timestamp) {
        dtEl.textContent = latest.timestamp;
    }
}

function updateSoilBanner(latest) {
    if (!latest) return;

    const badge = document.getElementById('soilStatusBadge');
    const advisory = document.getElementById('soilAdvisoryText');
    if (!badge || !advisory) return;
    const ph = parseFloat(latest.ph_ai);

    badge.className = 'soil-status-pill';

    if (ph < 5.0) {
        badge.classList.add('status-acidic');
        badge.innerHTML = '● CRITICAL: VERY ACIDIC';
        advisory.textContent = 'ดินมีสภาพกรดรุนแรง ธาตุอะลูมิเนียมเป็นพิษ ขัดขวางการดูดซึม N-P-K ของทุเรียน แนะนำให้หว่านปูนโดโลไมต์ปรับสภาพดินทันที';
    } else if (ph >= 5.5 && ph <= 6.5) {
        badge.classList.add('status-optimal');
        badge.innerHTML = '★ OPTIMAL DURIAN SOIL';
        advisory.textContent = 'ระดับความเป็นกรด-ด่างอยู่ในช่วงสมบูรณ์แบบที่สุดสำหรับการเจริญเติบโตและการสะสมอาหารของทุเรียนภาคตะวันออก';
    } else if (ph > 6.5 && ph <= 7.5) {
        badge.classList.add('status-neutral');
        badge.innerHTML = '● NEUTRAL SOIL';
        advisory.textContent = 'สภาพดินเป็นกลาง เหมาะสมสำหรับพืชไร่และพืชสวนทั่วไป มีความสมดุลของจุลินทรีย์ในดิน';
    } else {
        badge.classList.add('status-alkaline');
        badge.innerHTML = '▲ ALKALINE SOIL';
        advisory.textContent = 'ดินมีสภาพเป็นด่าง อาจทำให้พืชขาดธาตุอาหารรอง เช่น สังกะสีและเหล็ก ควรเติมอินทรียวัตถุหรือปุ๋ยหมัก';
    }
}

function updateChartData(history) {
    if (!phChart || !history || history.length === 0) return;

    const labels = history.map(item => {
        const parts = item.timestamp.split(' ');
        const dateShort = parts[0] ? parts[0].substring(5) : '';
        const timeShort = parts[1] ? parts[1].substring(0, 5) : '';
        return `${dateShort} ${timeShort}`;
    });

    const aiData = history.map(item => parseFloat(item.ph_ai));
    const tradData = history.map(item => parseFloat(item.ph_traditional));

    phChart.data.labels = labels;
    phChart.data.datasets[0].data = aiData;
    phChart.data.datasets[1].data = tradData;
    phChart.update('none'); // silent update without animation lag
}

function updateTable(history) {
    const tbody = document.getElementById('tableBody');
    if (!tbody || !history) return;

    // เก็บ 10 รายการล่าสุด
    const latestItems = [...history].reverse().slice(0, 10);
    
    let html = '';
    latestItems.forEach(item => {
        const isNew = item.id > lastKnownId;
        const highlightClass = isNew ? 'row-highlight' : '';
        html += `
            <tr class="${highlightClass}">
                <td>#${item.id}</td>
                <td><span style="font-family:'JetBrains Mono'; font-weight:600; color:#38bdf8; font-size:0.85rem;">${item.timestamp}</span></td>
                <td><span style="font-size:0.75rem; font-weight:700; padding:2px 8px; border-radius:4px; background:rgba(56,189,248,0.15); color:#38bdf8;">${item.mode || 'FIELD_RUN'}</span></td>
                <td><span style="font-size:0.75rem; font-weight:700; padding:2px 8px; border-radius:4px; background:rgba(255,210,50,0.15); color:#ffd232;">${item.model_name || 'ANN DURIAN'}</span></td>
                <td style="color: var(--color-green); font-weight:700;">${Number(item.ph_ai).toFixed(2)}</td>
                <td style="color: var(--color-orange);">${Number(item.ph_traditional).toFixed(2)}</td>
                <td>${Number(item.voltage).toFixed(3)} V</td>
                <td>${Number(item.temp_c).toFixed(1)} °C</td>
            </tr>
        `;
    });

    tbody.innerHTML = html;

    if (history.length > 0) {
        lastKnownId = history[history.length - 1].id;
    }
}

function setOnlineStatus(online) {
    const badge = document.getElementById('systemStatusBadge');
    if (!badge) return;
    if (online) {
        badge.innerHTML = '<span class="pulse-dot"></span> LIVE CONNECTED';
        badge.style.color = 'var(--color-green)';
        badge.style.borderColor = 'rgba(0, 230, 130, 0.35)';
    } else {
        badge.innerHTML = '<span class="pulse-dot" style="background:#ff5252; box-shadow:none;"></span> OFFLINE / WAITING';
        badge.style.color = '#ff5252';
        badge.style.borderColor = 'rgba(255, 82, 82, 0.35)';
    }
}

// ----------------- Polling & Actions -----------------
function startPolling() {
    if (pollTimer) clearInterval(pollTimer);
    pollTimer = setInterval(fetchDashboardData, POLL_INTERVAL);
    isPolling = true;
    updatePollBtn();
}

function stopPolling() {
    if (pollTimer) clearInterval(pollTimer);
    isPolling = false;
    updatePollBtn();
}

function togglePolling() {
    if (isPolling) {
        stopPolling();
    } else {
        startPolling();
        fetchDashboardData();
    }
}

function updatePollBtn() {
    const btn = document.getElementById('btnTogglePoll');
    if (isPolling) {
        btn.innerHTML = '⏸ Pause Stream';
    } else {
        btn.innerHTML = '▶ Resume Stream';
    }
}

// ฟังก์ชันจำลองการส่งข้อมูล (สำหรับทดสอบหน้าเว็บโดยไม่ต้องต่อฮาร์ดแวร์จริง)
async function simulateSample() {
    const randomVolt = 1.62 + (Math.random() * 0.1);
    const randomTemp = 24.5 + (Math.random() * 2.0);
    const randomPHTrad = 7.00 + ((1.65 - randomVolt) * 3.5);
    const randomPHAI = 6.95 + ((1.65 - randomVolt) * 3.4);

    try {
        const resp = await fetch('api/post_data.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                voltage: randomVolt,
                temp_c: randomTemp,
                ph_traditional: randomPHTrad,
                ph_ai: randomPHAI,
                target_buffer: 'SIMULATED',
                sample_note: 'WEB_TEST_CLICK'
            })
        });
        const res = await resp.json();
        if (res.success) {
            fetchDashboardData();
        }
    } catch (e) {
        console.error('Simulation error:', e);
    }
}
