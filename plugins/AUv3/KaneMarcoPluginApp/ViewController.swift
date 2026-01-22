//
//  ViewController.swift
//  NaturePluginApp
//
//  View controller for Nature Marco AUv3 host app
//

import UIKit
import AVFoundation

class ViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()

        view.backgroundColor = .systemBackground

        // Setup UI
        setupUI()
    }

    private func setupUI() {
        let stackView = UIStackView()
        stackView.axis = .vertical
        stackView.spacing = 20
        stackView.translatesAutoresizingMaskIntoConstraints = false

        // Title label
        let titleLabel = UILabel()
        titleLabel.text = "Nature Marco"
        titleLabel.font = .systemFont(ofSize: 32, weight: .bold)
        titleLabel.textAlignment = .center

        // Subtitle label
        let subtitleLabel = UILabel()
        subtitleLabel.text = "Hybrid Virtual Analog Synthesizer"
        subtitleLabel.font = .systemFont(ofSize: 18, weight: .regular)
        subtitleLabel.textAlignment = .center
        subtitleLabel.textColor = .secondaryLabel

        // Info label
        let infoLabel = UILabel()
        infoLabel.text = "Open this app in GarageBand, AUM, or other AUv3 host to use Nature Marco as an instrument plugin."
        infoLabel.font = .systemFont(ofSize: 14, weight: .regular)
        infoLabel.textAlignment = .center
        infoLabel.numberOfLines = 0
        infoLabel.textColor = .secondaryLabel

        // Feature list
        let featuresLabel = UILabel()
        featuresLabel.text = """
        Features:
        • Oscillator WARP phase manipulation
        • FM synthesis with carrier/modulator swap
        • 16-slot modulation matrix
        • 8 macro controls
        • Multimode filter
        • Sub-oscillator and noise generator
        • 4 LFOs with multiple waveforms
        • 16-voice polyphony
        """
        featuresLabel.font = .systemFont(ofSize: 14, weight: .regular)
        featuresLabel.numberOfLines = 0
        featuresLabel.textAlignment = .left

        // Add to stack
        stackView.addArrangedSubview(titleLabel)
        stackView.addArrangedSubview(subtitleLabel)
        stackView.addArrangedSubview(UIViewspacer(height: 40))
        stackView.addArrangedSubview(infoLabel)
        stackView.addArrangedSubview(UIViewspacer(height: 40))
        stackView.addArrangedSubview(featuresLabel)

        view.addSubview(stackView)

        // Layout constraints
        NSLayoutConstraint.activate([
            stackView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            stackView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            stackView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 40),
            stackView.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -40)
        ])
    }
}

// Helper for spacing in UIStackView
class UIViewspacer: UIView {
    init(height: CGFloat) {
        super.init(frame: .zero)
        NSLayoutConstraint.activate([
            heightAnchor.constraint(equalToConstant: height)
        ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
