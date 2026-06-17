//
//  HomeBotApp.swift
//  HomeBot
//
//  Created by Digkill on 23.05.2026.
//

import SwiftUI

@main
struct HomeBotApp: App {
    @StateObject private var session = HomeBotSession()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(session)
        }
    }
}
