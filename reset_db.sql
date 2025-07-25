-- detections(감지 이미지&텍스트 테이블)을 제외한 모두 테이블 delete sql문

SELECT '--- Current Data ---';

select * from lines;
select * from baseLines;
select * from verticalLineEquations;

delete from lines;
delete from baseLines;
delete from verticalLineEquations;
delete from accounts;

CREATE TABLE IF NOT EXISTS recovery_codes (
    id TEXT NOT NULL,
    code TEXT NOT NULL,
    used INTEGER DEFAULT 0,
    FOREIGN KEY(id) REFERENCES accounts(id)
);

ALTER TABLE accounts ADD COLUMN otp_secret TEXT;

SELECT 'Reset Complete';