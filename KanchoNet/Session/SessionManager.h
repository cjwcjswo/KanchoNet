#pragma once

#include "../Types.h"
#include "Session.h"
#include "../Utils/NonCopyable.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

namespace KanchoNet
{
    // 세션 컨테이너 관리 클래스
    class SessionManager : public NonCopyable
    {
    public:
        // public 멤버변수 (없음)
        
    private:
        // private 멤버변수
        uint32_t mMaxSessions;
        std::atomic<SessionID> mNextSessionID;
        
        std::unordered_map<SessionID, std::unique_ptr<Session>> mSessions;
        mutable std::mutex mMutex;
        
    public:
        // 생성자, 파괴자
        explicit SessionManager(uint32_t maxSessions);
        ~SessionManager();
        
    public:
        // public 함수
        // 세션 추가
        Session* AddSession(SocketHandle socket, const SessionConfig& config);
        
        // 세션 제거
        bool RemoveSession(SessionID sessionID);
        
        // 세션 검색
        Session* GetSession(SessionID sessionID);
        const Session* GetSession(SessionID sessionID) const;
        
        // 세션 존재 확인
        bool HasSession(SessionID sessionID) const;
        
        // 전체 세션 순회
        void ForEachSession(std::function<void(Session*)> callback);
        
        // 상태 정보
        size_t GetSessionCount() const;
        size_t GetMaxSessions() const { return mMaxSessions; }
        bool IsFull() const { return GetSessionCount() >= mMaxSessions; }
        
        // 전체 세션 제거
        void Clear();

    private:
        // private 함수
        SessionID GenerateSessionID();
    };

} // namespace KanchoNet

