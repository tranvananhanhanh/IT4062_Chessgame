const stats = [
  { label: 'Playing now', value: '200,407' },
  { label: 'Games today', value: '19,254,346' },
  { label: 'Moves this minute', value: '812,991' },
]

const Header = () => (
  <header className="stats-bar">
    {stats.map((stat) => (
      <div key={stat.label} className="stat-item">
        <p className="stat-value">{stat.value}</p>
        <span>{stat.label}</span>
      </div>
    ))}
  </header>
)

export default Header
