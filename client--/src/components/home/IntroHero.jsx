const IntroHero = ({ gameMessage, onGetStarted, onLearnMore }) => {
  return (
    <>
      <section className="hero-section">
        <div className="hero-text">
          <p className="hero-label">IT4062 chess network</p>
          <h1>Play chess. Improve your game. Have fun!</h1>
          <p>
            Đăng nhập để tìm đối thủ tức thì, luyện tập các thế trận mới và theo dõi cộng đồng
            chơi cờ năng động nhất của học phần IT4062.
          </p>
          <div className="hero-actions">
            <button type="button" className="primary-cta" onClick={onGetStarted}>
              Get Started
            </button>
            <button type="button" className="ghost-cta" onClick={onLearnMore}>
              Learn More
            </button>
          </div>
        </div>
        <div className="hero-visual" aria-hidden="true">
          <div className="hero-floor" />
          <div className="hero-piece hero-king" />
          <div className="hero-piece hero-knight" />
          <div className="hero-piece hero-pawn" />
        </div>
      </section>

      <section className="prestart-panel">
        <h2>Sẵn sàng tranh tài</h2>
        <p>Tạo tài khoản hoặc đăng nhập để lưu lại lịch sử và thống kê nước đi của bạn.</p>
        <p>Nhấn Get Started để mở ngay một bàn cờ thử nghiệm.</p>
        <div className="prestart-status">{gameMessage}</div>
      </section>
    </>
  )
}

export default IntroHero
