/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: js/dashboard.js
 * Description: Real-time UI updating engine, Chart.js integration, and API polling
 */

let phChart = null;
let isPolling = true;
let pollTimer = null;
let lastKnownId = 0;

const POLL_INTERVAL = 2000; // ทุก 2 วินาที

document.addEventListener('DOMContentLoaded', () => {
    initChart();
    fetchDashboardData();
    startPolling();

    // Event Listeners
    document.getElementById('btnTogglePoll').addEventListener('click', togglePolling);
    document.getElementById('btnSimulate').addEventListener('click', simulateSample);
});

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
        const response = await fetch('api/get_data.php?limit=30');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        const result = await response.json();

        if (result.success) {
            updateKPIs(result.latest, result.stats);
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

function updateKPIs(latest, stats) {
    if (!latest) return;

    // AI Predicted pH
    const aiEl = document.getElementById('kpiAIPH');
    aiEl.textContent = Number(latest.ph_ai).toFixed(2);

    // Traditional Nernst pH
    const tradEl = document.getElementById('kpiTradPH');
    tradEl.textContent = Number(latest.ph_traditional).toFixed(2);

    // Delta Compensation Tag
    const deltaEl = document.getElementById('kpiDeltaTag');
    const delta = Number(latest.delta_error);
    deltaEl.textContent = (delta >= 0 ? '+' : '') + delta.toFixed(3) + ' pH';

    // Cell Potential & Temperature
    document.getElementById('kpiVoltage').textContent = Number(latest.voltage).toFixed(3) + ' V';
    document.getElementById('kpiTemp').textContent = Number(latest.temp_c).toFixed(1) + ' °C';

    // Total Count & Buffer
    document.getElementById('kpiTotalRows').textContent = stats.total_count;
    document.getElementById('kpiTargetBuf').textContent = latest.target_buffer || 'FIELD';
    document.getElementById('kpiOptimalPercent').textContent = stats.optimal_percentage + '%';
}

function updateSoilBanner(latest) {
    if (!latest) return;

    const badge = document.getElementById('soilStatusBadge');
    const advisory = document.getElementById('soilAdvisoryText');
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
        const timePart = item.timestamp.split(' ')[1] || item.timestamp;
        return timePart.substring(0, 5);
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
                <td>${item.timestamp.split(' ')[1] || item.timestamp}</td>
                <td style="color: var(--color-green); font-weight:700;">${Number(item.ph_ai).toFixed(2)}</td>
                <td style="color: var(--color-orange);">${Number(item.ph_traditional).toFixed(2)}</td>
                <td>${Number(item.voltage).toFixed(3)} V</td>
                <td>${Number(item.temp_c).toFixed(1)} °C</td>
                <td><span style="font-size:0.75rem; color:var(--text-muted);">${item.target_buffer}</span></td>
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
