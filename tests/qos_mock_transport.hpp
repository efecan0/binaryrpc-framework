TEST_CASE("AtLeastOnce delivers with possible duplicates", "[qos]") {
    // 1) App + WebSocketTransport
    App app;
    auto ws = std::make_unique<WebSocketTransport>(app.getSessionManager(), 60);
    ReliableOptions opt{ QoSLevel::AtLeastOnce, 100, 3, 1000,
                         std::make_shared<LinearBackoff>(100ms, 1000ms)};
    ws->setReliable(opt);
    auto* wsPtr = ws.get();
    app.setTransport(std::move(ws));

    // 2) MockClient <-> WebSocketTransport pipe
    MockClient client(*wsPtr);

    // 3) echo handler, kaÃ§ kez tetiklendiÄŸini sayaÃ§la
    int called = 0;
    app.registerRPC("echo", [&](auto const&, RpcContext& ctx) {
        ++called;
        ctx.reply({1});   // 1â€‘byteÂ payload
    });

    // 4) Client ACKâ€™i ilk seferde DROPlasÄ±n -> retry
    client.setAckMode(DropFirst); // enum
    client.send("echo", {42});

    // eventâ€‘loop / tick
    app.pollFor(500ms);

    REQUIRE(called >= 1);   // en az bir kez handler Ã§aÄŸrÄ±lmalÄ±
    REQUIRE(called <= 2);   // AtLeastOnce â†’ duplicate 0 veya 1

    REQUIRE(client.receivedPayloads().front() == std::vector<uint8_t>{1});
}
