import { useEffect, useMemo, useState, type FormEvent, type ReactNode } from 'react'
import {
  AlertTriangle,
  Bell,
  Droplets,
  Gauge,
  History,
  LayoutDashboard,
  LogOut,
  Settings,
  SlidersHorizontal,
  Sun,
  Thermometer,
  Wind,
} from 'lucide-react'
import {
  Area,
  AreaChart,
  CartesianGrid,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { createUserWithEmailAndPassword, onAuthStateChanged, signInWithEmailAndPassword, signOut, type User } from 'firebase/auth'
import { limitToLast, onValue, query, ref, update } from 'firebase/database'
import { auth, db } from './firebase'
import './App.css'

type SensorEntry = {
  day: string
  soil: number
  temp: number
  humidity: number
  light: number
  health: number
}

type AlertType = 'critical' | 'warning' | 'info'

type AlertItem = {
  id: string
  type: AlertType
  sensor: string
  message: string
  value: string
  time: string
  acknowledged: boolean
}

type DeviceStatus = {
  diagnosis: string
  recommendation: string
  pumpActive: boolean
  healthScore: number
}

type Thresholds = {
  soilMin: number
  soilMax: number
  tempMin: number
  tempMax: number
  humidityMin: number
  humidityMax: number
  lightMin: number
  lightMax: number
}

const DEVICE_PATH = 'AgriBloom/device_001'

const fallbackSensorHistory: SensorEntry[] = [
  { day: 'Mar 30', soil: 72, temp: 22.1, humidity: 64, light: 4200, health: 80 },
  { day: 'Mar 31', soil: 74, temp: 23.2, humidity: 65, light: 3580, health: 81 },
  { day: 'Apr 1', soil: 67, temp: 24.3, humidity: 68, light: 4100, health: 83 },
  { day: 'Apr 2', soil: 69, temp: 25, humidity: 66, light: 4480, health: 84 },
  { day: 'Apr 3', soil: 76, temp: 24.2, humidity: 67, light: 3810, health: 85 },
  { day: 'Apr 4', soil: 74, temp: 23.8, humidity: 73, light: 5020, health: 86 },
  { day: 'Apr 5', soil: 68, temp: 23.3, humidity: 72, light: 4782, health: 84 },
]

const fallbackAlerts: AlertItem[] = [
  {
    id: 'fallback-1',
    type: 'warning',
    sensor: 'Soil Moisture',
    message: 'Soil moisture is below optimal range',
    value: '52 (Threshold: 60)',
    time: 'Apr 5, 10:41 AM',
    acknowledged: false,
  },
  {
    id: 'fallback-2',
    type: 'info',
    sensor: 'Pump',
    message: 'Auto-watering cycle completed successfully',
    value: 'Run time: 10s',
    time: 'Apr 5, 8:44 AM',
    acknowledged: true,
  },
  {
    id: 'fallback-3',
    type: 'critical',
    sensor: 'Temperature',
    message: 'Temperature approaching upper limit',
    value: '26.6 (Threshold: 27)',
    time: 'Apr 5, 9:46 AM',
    acknowledged: false,
  },
  {
    id: 'fallback-4',
    type: 'warning',
    sensor: 'Light',
    message: 'Light intensity dropped below minimum',
    value: '2800 (Threshold: 5000)',
    time: 'Apr 5, 6:45 AM',
    acknowledged: false,
  },
]

const defaultThresholds: Thresholds = {
  soilMin: 60,
  soilMax: 70,
  tempMin: 18,
  tempMax: 27,
  humidityMin: 50,
  humidityMax: 80,
  lightMin: 5000,
  lightMax: 5500,
}

const navItems: { key: string; label: string; icon: ReactNode }[] = [
  { key: 'dashboard', label: 'Dashboard', icon: <LayoutDashboard size={16} /> },
  { key: 'history', label: 'History', icon: <History size={16} /> },
  { key: 'alerts', label: 'Alerts', icon: <Bell size={16} /> },
  { key: 'control', label: 'Control', icon: <SlidersHorizontal size={16} /> },
  { key: 'settings', label: 'Settings', icon: <Settings size={16} /> },
]

const toNumber = (value: unknown, fallback = 0) => {
  if (typeof value === 'number' && Number.isFinite(value)) return value
  return fallback
}

const formatTimestamp = (input: unknown) => {
  if (typeof input === 'number' && Number.isFinite(input)) {
    if (input < 1000000000000) return `T+${Math.round(input / 1000)}s`
    return new Date(input).toLocaleString()
  }
  if (typeof input === 'string') return input
  return 'Just now'
}

function App() {
  const [authUser, setAuthUser] = useState<User | null>(null)
  const [authLoading, setAuthLoading] = useState(true)
  const [authMode, setAuthMode] = useState<'signin' | 'signup'>('signin')
  const [authError, setAuthError] = useState('')
  const [email, setEmail] = useState('farmer@agribloom.com')
  const [password, setPassword] = useState('')

  const [activePage, setActivePage] = useState('dashboard')
  const [runtime, setRuntime] = useState(10)
  const [autoWatering, setAutoWatering] = useState(true)
  const [notifications, setNotifications] = useState(true)
  const [alertFilter, setAlertFilter] = useState<'all' | AlertType>('all')
  const [wateringNow, setWateringNow] = useState(false)
  const [dataError, setDataError] = useState('')

  const [alerts, setAlerts] = useState<AlertItem[]>(fallbackAlerts)
  const [sensorData, setSensorData] = useState<SensorEntry[]>(fallbackSensorHistory)
  const [status, setStatus] = useState<DeviceStatus>({
    diagnosis: 'Unknown',
    recommendation: 'Monitor conditions',
    pumpActive: false,
    healthScore: fallbackSensorHistory[fallbackSensorHistory.length - 1].health,
  })
  const [thresholds, setThresholds] = useState<Thresholds>(defaultThresholds)

  useEffect(() => {
    const unsubscribe = onAuthStateChanged(auth, (user) => {
      setAuthUser(user)
      setAuthLoading(false)
    })
    return unsubscribe
  }, [])

  useEffect(() => {
    if (!authUser) {
      return
    }
    const sensorsRef = ref(db, `${DEVICE_PATH}/sensors`)
    const statusRef = ref(db, `${DEVICE_PATH}/status`)
    const alertsRef = query(ref(db, `${DEVICE_PATH}/alerts`), limitToLast(50))
    const historyRef = query(ref(db, `${DEVICE_PATH}/history`), limitToLast(30))
    const controlRef = ref(db, `${DEVICE_PATH}/control`)
    const settingsRef = ref(db, `${DEVICE_PATH}/settings`)

    const unsubSensors = onValue(
      sensorsRef,
      (snapshot) => {
        const value = snapshot.val() as Record<string, unknown> | null
        if (!value) return
        setSensorData((prev) => {
          if (prev.length > 0) {
            const latest = prev[prev.length - 1]
            return [
              ...prev.slice(0, -1),
              {
                ...latest,
                soil: toNumber(value.moisturePct, latest.soil),
                temp: toNumber(value.temperature, latest.temp),
                humidity: toNumber(value.humidity, latest.humidity),
                light: toNumber(value.lux, latest.light),
              },
            ]
          }
          return prev
        })
      },
      (error) => setDataError(error.message),
    )

    const unsubStatus = onValue(
      statusRef,
      (snapshot) => {
        const value = snapshot.val() as Record<string, unknown> | null
        if (!value) return
        const pumpActive = Boolean(value.pumpActive)
        setStatus({
          diagnosis: typeof value.diagnosis === 'string' ? value.diagnosis : 'Unknown',
          recommendation:
            typeof value.recommendation === 'string'
              ? value.recommendation
              : 'Monitor conditions',
          pumpActive,
          healthScore: toNumber(value.healthScore, 80),
        })
        setWateringNow(pumpActive)
      },
      (error) => setDataError(error.message),
    )

    const unsubAlerts = onValue(
      alertsRef,
      (snapshot) => {
        const value = snapshot.val() as Record<string, Record<string, unknown>> | null
        if (!value) {
          setAlerts([])
          return
        }
        const normalized = Object.entries(value)
          .map(([id, entry]) => {
            const type: AlertType =
              entry.type === 'critical' || entry.type === 'warning' || entry.type === 'info'
                ? entry.type
                : 'info'
            return {
              id,
              type,
              sensor: typeof entry.sensor === 'string' ? entry.sensor : 'System',
              message: typeof entry.message === 'string' ? entry.message : 'System update',
              value: typeof entry.value === 'string' ? entry.value : '-',
              time: formatTimestamp(entry.time ?? entry.createdAt),
              acknowledged: Boolean(entry.acknowledged),
              createdAt: toNumber(entry.createdAt, 0),
            }
          })
          .sort((a, b) => b.createdAt - a.createdAt)
          .map((item) => {
            const { createdAt, ...rest } = item
            void createdAt
            return rest
          })
        setAlerts(normalized)
      },
      (error) => setDataError(error.message),
    )

    const unsubHistory = onValue(
      historyRef,
      (snapshot) => {
        const value = snapshot.val() as Record<string, Record<string, unknown>> | null
        if (!value) {
          setSensorData(fallbackSensorHistory)
          return
        }

        const points = Object.entries(value)
          .map(([key, entry]) => {
            const numericKey = Number(key)
            const createdAt = toNumber(entry.createdAt, Number.isFinite(numericKey) ? numericKey : Date.now())
            return {
              createdAt,
              day: new Date(createdAt).toLocaleDateString('en-US', { month: 'short', day: 'numeric' }),
              soil: toNumber(entry.moisturePct, toNumber(entry.soil, 0)),
              temp: toNumber(entry.temperature, toNumber(entry.temp, 0)),
              humidity: toNumber(entry.humidity, 0),
              light: toNumber(entry.lux, toNumber(entry.light, 0)),
              health: toNumber(entry.health, 80),
            }
          })
          .sort((a, b) => a.createdAt - b.createdAt)
          .map((chartEntry) => {
            const { createdAt, ...rest } = chartEntry
            void createdAt
            return rest
          })

        if (points.length > 0) {
          setSensorData(points)
        }
      },
      (error) => setDataError(error.message),
    )

    const unsubControl = onValue(controlRef, (snapshot) => {
      const value = snapshot.val() as Record<string, unknown> | null
      if (!value) return
      setAutoWatering(Boolean(value.autoWatering))
      const nextRuntime = toNumber(value.pumpDurationSeconds, 10)
      if (nextRuntime >= 1 && nextRuntime <= 12) {
        setRuntime(nextRuntime)
      }
    })

    const unsubSettings = onValue(settingsRef, (snapshot) => {
      const value = snapshot.val() as Record<string, unknown> | null
      if (!value) return
      setNotifications(Boolean(value.notificationsEnabled))
      const remoteThresholds = value.thresholds as Record<string, unknown> | undefined
      if (!remoteThresholds) return
      setThresholds({
        soilMin: toNumber(remoteThresholds.soilMin, defaultThresholds.soilMin),
        soilMax: toNumber(remoteThresholds.soilMax, defaultThresholds.soilMax),
        tempMin: toNumber(remoteThresholds.tempMin, defaultThresholds.tempMin),
        tempMax: toNumber(remoteThresholds.tempMax, defaultThresholds.tempMax),
        humidityMin: toNumber(remoteThresholds.humidityMin, defaultThresholds.humidityMin),
        humidityMax: toNumber(remoteThresholds.humidityMax, defaultThresholds.humidityMax),
        lightMin: toNumber(remoteThresholds.lightMin, defaultThresholds.lightMin),
        lightMax: toNumber(remoteThresholds.lightMax, defaultThresholds.lightMax),
      })
    })

    return () => {
      unsubSensors()
      unsubStatus()
      unsubAlerts()
      unsubHistory()
      unsubControl()
      unsubSettings()
    }
  }, [authUser])

  const currentSensors =
    sensorData.length > 0
      ? sensorData[sensorData.length - 1]
      : fallbackSensorHistory[fallbackSensorHistory.length - 1]

  const filteredAlerts = useMemo(() => {
    if (alertFilter === 'all') return alerts
    return alerts.filter((alert) => alert.type === alertFilter)
  }, [alertFilter, alerts])

  const handleAuth = async (event: FormEvent) => {
    event.preventDefault()
    setAuthError('')
    try {
      if (authMode === 'signin') {
        await signInWithEmailAndPassword(auth, email, password)
      } else {
        await createUserWithEmailAndPassword(auth, email, password)
      }
    } catch (error) {
      setAuthError(error instanceof Error ? error.message : 'Authentication failed')
    }
  }

  const acknowledgeAlert = async (id: string, acknowledged: boolean) => {
    try {
      await update(ref(db, `${DEVICE_PATH}/alerts/${id}`), { acknowledged: !acknowledged })
    } catch (error) {
      setDataError(error instanceof Error ? error.message : 'Unable to update alert')
    }
  }

  const waterNow = async () => {
    if (!authUser) return
    try {
      setDataError('')
      setWateringNow(true)
      await update(ref(db, `${DEVICE_PATH}/control`), {
        manualWaterNow: true,
        manualWaterRequestedAt: Date.now(),
        manualWaterRequestedBy: authUser.email ?? 'unknown',
        pumpDurationSeconds: runtime,
      })
    } catch (error) {
      setWateringNow(false)
      setDataError(error instanceof Error ? error.message : 'Manual watering command failed')
    }
  }

  const updateAutoWatering = async (nextValue: boolean) => {
    setAutoWatering(nextValue)
    try {
      await update(ref(db, `${DEVICE_PATH}/control`), { autoWatering: nextValue })
    } catch (error) {
      setDataError(error instanceof Error ? error.message : 'Unable to update auto-watering')
    }
  }

  const saveSettings = async () => {
    try {
      await Promise.all([
        update(ref(db, `${DEVICE_PATH}/control`), {
          autoWatering,
          pumpDurationSeconds: runtime,
        }),
        update(ref(db, `${DEVICE_PATH}/settings`), {
          notificationsEnabled: notifications,
          thresholds,
        }),
      ])
    } catch (error) {
      setDataError(error instanceof Error ? error.message : 'Saving settings failed')
    }
  }

  if (authLoading) {
    return (
      <div className="login-shell">
        <div className="login-card">
          <h1>Agri Bloom</h1>
          <p>Loading account...</p>
        </div>
      </div>
    )
  }

  if (!authUser) {
    return (
      <div className="login-shell">
        <form className="login-card" onSubmit={handleAuth}>
          <div className="brand-emoji">🍅</div>
          <h1>Agri Bloom</h1>
          <p>Tomato Seedling Monitor</p>
          <label htmlFor="email">Email</label>
          <input
            id="email"
            type="email"
            value={email}
            onChange={(event) => setEmail(event.target.value)}
            required
          />
          <label htmlFor="password">Password</label>
          <input
            id="password"
            type="password"
            value={password}
            onChange={(event) => setPassword(event.target.value)}
            minLength={6}
            required
          />
          <button className="sign-in" type="submit">
            {authMode === 'signin' ? 'Sign In' : 'Create Account'}
          </button>
          <button
            type="button"
            onClick={() => {
              setAuthError('')
              setAuthMode((prev) => (prev === 'signin' ? 'signup' : 'signin'))
            }}
          >
            {authMode === 'signin'
              ? 'Need an account? Create one'
              : 'Already have an account? Sign in'}
          </button>
          {authError && <small>{authError}</small>}
        </form>
      </div>
    )
  }

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="logo-wrap">
          <div className="logo-dot">🍅</div>
          <div>
            <h2>Agri.Bloom</h2>
            <span>Tomato Seedling Monitor</span>
          </div>
        </div>
        <nav>
          {navItems.map(({ key, label, icon }) => (
            <button
              key={key}
              className={activePage === key ? 'nav-link active' : 'nav-link'}
              onClick={() => setActivePage(key)}
            >
              {icon}
              {label}
            </button>
          ))}
        </nav>
        <button
          className="logout"
          onClick={async () => {
            await signOut(auth)
          }}
        >
          <LogOut size={16} />
          Logout
        </button>
      </aside>

      <main className="content">
        <header className="page-head">
          <div>
            <h1>
              {activePage === 'dashboard' && 'Greenhouse Overview'}
              {activePage === 'history' && 'Historical Data'}
              {activePage === 'alerts' && 'System Alerts'}
              {activePage === 'control' && 'Device Control'}
              {activePage === 'settings' && 'System Settings'}
            </h1>
            <p>{status.recommendation}</p>
          </div>
          <button className="water-now-top" onClick={waterNow} disabled={wateringNow}>
            <Droplets size={16} />
            {wateringNow ? 'Pumping...' : 'Water Now'}
          </button>
        </header>

        {(activePage === 'dashboard' || activePage === 'history') && (
          <section className="metric-grid">
            <article className="metric-card">
              <Droplets size={18} />
              <p>Soil Moisture</p>
              <strong>{Math.round(currentSensors.soil)}%</strong>
            </article>
            <article className="metric-card">
              <Thermometer size={18} />
              <p>Temperature</p>
              <strong>{currentSensors.temp.toFixed(1)}°C</strong>
            </article>
            <article className="metric-card">
              <Wind size={18} />
              <p>Humidity</p>
              <strong>{Math.round(currentSensors.humidity)}%</strong>
            </article>
            <article className="metric-card">
              <Sun size={18} />
              <p>Light Intensity</p>
              <strong>{Math.round(currentSensors.light)} lx</strong>
            </article>
            <article className="metric-card">
              <Gauge size={18} />
              <p>Health Score</p>
              <strong>{Math.round(status.healthScore || currentSensors.health)}</strong>
            </article>
          </section>
        )}

        {activePage === 'dashboard' && (
          <section className="dashboard-grid">
            <article className="panel">
              <h3>AI Plant Health Analysis</h3>
              <p>{status.diagnosis}</p>
              <div className="score">
                <span>{Math.round(status.healthScore || currentSensors.health)}</span>
                <small>/100</small>
              </div>
              <div className="progress">
                <div style={{ width: `${Math.round(status.healthScore || currentSensors.health)}%` }} />
              </div>
              <p>{status.recommendation}</p>
            </article>
            <article className="panel">
              <h3>Quick Actions</h3>
              <button onClick={waterNow} disabled={wateringNow}>
                Manual Water ({runtime}s)
              </button>
              <button disabled>Toggle Grow Lights</button>
              <button disabled>Activate Ventilation</button>
            </article>
          </section>
        )}

        {activePage === 'history' && (
          <section className="chart-grid">
            <article className="panel chart-panel">
              <h3>Soil Moisture (%)</h3>
              <ResponsiveContainer width="100%" height={220}>
                <AreaChart data={sensorData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="day" />
                  <YAxis />
                  <Tooltip />
                  <Area type="monotone" dataKey="soil" stroke="#1a8f3a" fill="#c9edd3" />
                </AreaChart>
              </ResponsiveContainer>
            </article>
            <article className="panel chart-panel">
              <h3>Temperature (°C)</h3>
              <ResponsiveContainer width="100%" height={220}>
                <LineChart data={sensorData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="day" />
                  <YAxis />
                  <Tooltip />
                  <Line type="monotone" dataKey="temp" stroke="#1787d3" strokeWidth={2} />
                </LineChart>
              </ResponsiveContainer>
            </article>
            <article className="panel chart-panel">
              <h3>Humidity (%)</h3>
              <ResponsiveContainer width="100%" height={220}>
                <AreaChart data={sensorData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="day" />
                  <YAxis />
                  <Tooltip />
                  <Area type="monotone" dataKey="humidity" stroke="#67a77f" fill="#d6efdd" />
                </AreaChart>
              </ResponsiveContainer>
            </article>
            <article className="panel chart-panel">
              <h3>Light Intensity (lx)</h3>
              <ResponsiveContainer width="100%" height={220}>
                <LineChart data={sensorData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="day" />
                  <YAxis />
                  <Tooltip />
                  <Line type="monotone" dataKey="light" stroke="#8474d1" strokeWidth={2} />
                </LineChart>
              </ResponsiveContainer>
            </article>
            <article className="panel chart-panel full">
              <h3>Plant Health Score</h3>
              <ResponsiveContainer width="100%" height={230}>
                <AreaChart data={sensorData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="day" />
                  <YAxis />
                  <Tooltip />
                  <Area type="monotone" dataKey="health" stroke="#1a8f3a" fill="#dcf5e3" />
                </AreaChart>
              </ResponsiveContainer>
            </article>
          </section>
        )}

        {activePage === 'alerts' && (
          <section className="panel">
            <div className="alert-head">
              <h3>Recent Activity</h3>
              <div className="chip-group">
                {(['all', 'critical', 'warning', 'info'] as const).map((type) => (
                  <button
                    key={type}
                    className={alertFilter === type ? 'chip active' : 'chip'}
                    onClick={() => setAlertFilter(type)}
                  >
                    {type}
                  </button>
                ))}
              </div>
            </div>
            <div className="alert-list">
              {filteredAlerts.map((alert) => (
                <article key={alert.id} className="alert-item">
                  <div className={`alert-icon ${alert.type}`}>
                    <AlertTriangle size={16} />
                  </div>
                  <div>
                    <strong>{alert.message}</strong>
                    <p>
                      {alert.value} · {alert.sensor}
                    </p>
                  </div>
                  <small>{alert.time}</small>
                  <button onClick={() => acknowledgeAlert(alert.id, alert.acknowledged)}>
                    {alert.acknowledged ? 'Acknowledged' : 'Acknowledge'}
                  </button>
                </article>
              ))}
            </div>
          </section>
        )}

        {dataError && (
          <section className="panel loading-panel">
            <p>{dataError}</p>
          </section>
        )}

        {activePage === 'control' && (
          <section className="control-stack">
            <article className="panel">
              <h3>Irrigation Pump</h3>
              <p>Manually trigger the water pump for a specific duration</p>
              <label htmlFor="runtime">Runtime Duration: {runtime}s</label>
              <input
                id="runtime"
                type="range"
                min="1"
                max="12"
                value={runtime}
                onChange={(event) => setRuntime(Number(event.target.value))}
              />
              <button className="primary" onClick={waterNow} disabled={wateringNow}>
                {wateringNow ? 'Watering...' : 'Start Watering Now'}
              </button>
            </article>
            <article className="panel">
              <h3>Automation</h3>
              <div className="toggle-row">
                <span>Auto-watering</span>
                <input
                  type="checkbox"
                  checked={autoWatering}
                  onChange={(event) => updateAutoWatering(event.target.checked)}
                />
              </div>
            </article>
          </section>
        )}

        {activePage === 'settings' && (
          <section className="control-stack">
            <article className="panel">
              <h3>Alert Thresholds</h3>
              <div className="input-grid">
                <label>
                  Soil Min (%)
                  <input
                    value={thresholds.soilMin}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, soilMin: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Soil Max (%)
                  <input
                    value={thresholds.soilMax}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, soilMax: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Temp Min (°C)
                  <input
                    value={thresholds.tempMin}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, tempMin: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Temp Max (°C)
                  <input
                    value={thresholds.tempMax}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, tempMax: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Humidity Min (%)
                  <input
                    value={thresholds.humidityMin}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, humidityMin: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Humidity Max (%)
                  <input
                    value={thresholds.humidityMax}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, humidityMax: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Light Min (lx)
                  <input
                    value={thresholds.lightMin}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, lightMin: Number(event.target.value) }))
                    }
                  />
                </label>
                <label>
                  Light Max (lx)
                  <input
                    value={thresholds.lightMax}
                    onChange={(event) =>
                      setThresholds((prev) => ({ ...prev, lightMax: Number(event.target.value) }))
                    }
                  />
                </label>
              </div>
            </article>
            <article className="panel">
              <h3>Automation & System</h3>
              <div className="toggle-row">
                <span>Auto-Watering</span>
                <input
                  type="checkbox"
                  checked={autoWatering}
                  onChange={(event) => updateAutoWatering(event.target.checked)}
                />
              </div>
              <label>
                Pump Duration (s): {runtime}
                <input
                  type="range"
                  min="1"
                  max="12"
                  value={runtime}
                  onChange={(event) => setRuntime(Number(event.target.value))}
                />
              </label>
              <div className="toggle-row">
                <span>System Notifications</span>
                <input
                  type="checkbox"
                  checked={notifications}
                  onChange={(event) => setNotifications(event.target.checked)}
                />
              </div>
              <button className="primary" onClick={saveSettings}>
                Save Changes
              </button>
            </article>
          </section>
        )}
      </main>
    </div>
  )
}

export default App
